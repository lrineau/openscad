#include "PolySetCGALEvaluator.h"
#include "cgal.h"
#include "cgalutils.h"

#include <CGAL/convex_hull_3.h>
#include <CGAL/Polygon_2_algorithms.h>

#include "polyset.h"
#include "CGALEvaluator.h"
#include "projectionnode.h"
#include "linearextrudenode.h"
#include "rotateextrudenode.h"
#include "cgaladvnode.h"
#include "rendernode.h"
#include "dxfdata.h"
#include "dxftess.h"
#include "module.h"

#include "printutils.h"
#include "openscad.h" // get_fragments_from_r()
#include <boost/foreach.hpp>
#include <vector>
#include <deque>

typedef CGAL_Nef_polyhedron3::Point_3 Point_3;

/*

This class converts multiple 3d-CGAL Nef polyhedrons into a single 2d by
stripping off the z coordinate of each face vertex and doing unions and
intersections. It uses the 'visitor' pattern from the CGAL manual.
Output is in the 'output_nefpoly2d' variable.

Note that the input 3d Nef polyhedron, as used here, is typically of
two types. The first is the result of an intersection between
the 3d Nef polyhedron and the xy-plane, with all z set to 0.

The second is the result of an intersection between the 3d nef
polyhedron and a very large, very thin box. This is used when CGAL
crashes during plane intersection. The thin box is used to 'simulate'
the xy plane. The result is that the 'top' of the box and the
'bottom' of the box will both contain 2d projections, quite similar
to what one would get at xy=0, but not exactly. these are then
unioned together.

Some key things to know about Nef Polyhedron2:

1. The 'mark' on a face is important when doing unions/intersections
2. The 'mark' can be non-deterministic based on the constructor.
 Possible factors include whether 'is_simple_2' returns true on the
 inputted points, and also perhaps the order of points fed to the constructor.

See also
http://www.cgal.org/Manual/latest/doc_html/cgal_manual/Nef_3/Chapter_main.html
http://www.cgal.org/Manual/latest/doc_html/cgal_manual/Nef_3_ref/Class_Nef_polyhedron3.html
OGL_helper.h
*/
class Flattener {
public:
	std::ostringstream out;
	CGAL_Nef_polyhedron2::Boundary boundary;
	shared_ptr<CGAL_Nef_polyhedron2> tmpnef2d;
	shared_ptr<CGAL_Nef_polyhedron2> output_nefpoly2d;
	CGAL::Direction_3<CGAL_Kernel3> up;
	bool debug;
	Flattener(bool debug=false)
	{
		output_nefpoly2d.reset( new CGAL_Nef_polyhedron2() );
		boundary = CGAL_Nef_polyhedron2::INCLUDED;
		up = CGAL::Direction_3<CGAL_Kernel3>(0,0,1);
		this->debug = debug;
	}
	std::string dump()
	{
		return out.str();
	}
	void visit( CGAL_Nef_polyhedron3::Vertex_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::Halfedge_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::SHalfedge_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::SHalfloop_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::SFace_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::Halffacet_const_handle hfacet ) {
		out.str("");
		out << " <!-- Halffacet visit -->\n";
		out << " <!-- mark:" << hfacet->mark() << " -->\n";
		if ( hfacet->plane().orthogonal_direction() != this->up ) {
			out << "\ndown facing half-facet. skipping\n";
			out << " <!-- Halffacet visit end-->\n";
			std::cout << out.str();
			return;
		}

		int contour_counter = 0;
		CGAL_Nef_polyhedron3::Halffacet_cycle_const_iterator i;
		CGAL_forall_facet_cycles_of( i, hfacet ) {
			CGAL_Nef_polyhedron3::SHalfedge_around_facet_const_circulator c1(i), c2(c1);
			std::list<CGAL_Nef_polyhedron2::Point> contour;
			CGAL_For_all( c1, c2 ) {
				CGAL_Nef_polyhedron3::Point_3 point3d = c1->source()->source()->point();
				CGAL_Nef_polyhedron2::Point point2d( point3d.x(), point3d.y() );
				contour.push_back( point2d );
			}
			tmpnef2d.reset( new CGAL_Nef_polyhedron2( contour.begin(), contour.end(), boundary ) );
			if ( contour_counter == 0 ) {
				out << "\n <!-- contour is a body. make union(). " << contour.size() << " points. -->\n" ;
				*(output_nefpoly2d) += *(tmpnef2d);
			} else {
				*(output_nefpoly2d) *= *(tmpnef2d);
				if (debug) out << "\n<!-- contour is a hole. make intersection(). " << contour.size() << " points. -->\n";
			}
      out << "\n<!-- ======== output tmp nef2d: ====== -->\n";
			out << dump_cgal_nef_polyhedron2_svg( *tmpnef2d );
      out << "\n<!-- ======== output accumulator: ==== -->\n";
			out << dump_cgal_nef_polyhedron2_svg( *output_nefpoly2d );
			contour_counter++;
		} // next facet cycle (i.e. next contour)
		out << " <!-- Halffacet visit end -->\n";
		std::cout << out.str();
	} // visit()
};



class Flattener2 {
public:
	std::ostringstream out;
	CGAL_Nef_polyhedron2::Boundary boundary;
	shared_ptr<CGAL_Nef_polyhedron2> tmpnef2d;
	shared_ptr<CGAL_Nef_polyhedron2> output_nefpoly2d;
	CGAL::Direction_3<CGAL_Kernel3> up;
	bool debug;
	Flattener2(bool debug=false)
	{
		output_nefpoly2d.reset( new CGAL_Nef_polyhedron2() );
		boundary = CGAL_Nef_polyhedron2::INCLUDED;
		up = CGAL::Direction_3<CGAL_Kernel3>(0,0,1);
		this->debug = debug;
	}
	std::string dump()
	{
		return out.str();
	}
	void visit( CGAL_Nef_polyhedron3::Vertex_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::Halfedge_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::SHalfedge_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::SHalfloop_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::SFace_const_handle ) {}
	void visit( CGAL_Nef_polyhedron3::Halffacet_const_handle hfacet ) {
		out.str("");
		out << " <!-- Halffacet visit -->\n";
		out << " <!-- mark:" << hfacet->mark() << " -->\n";
		if ( hfacet->plane().orthogonal_direction() != this->up ) {
			out << "\ndown facing half-facet. not skipping\n";
			out << " <!-- Halffacet visit end-->\n";
			std::cout << out.str();
			return;
		}

		CGAL_Nef_polyhedron3::Halffacet_cycle_const_iterator i;
/*		bool skip=false;
		CGAL_forall_facet_cycles_of( i, hfacet ) {
			CGAL_Nef_polyhedron3::SHalfedge_around_facet_const_circulator c1a(i), c2a(c1a);
			CGAL_For_all( c1a, c2a ) {
				CGAL_Nef_polyhedron3::Point_3 point3d = c1a->source()->source()->point();
				if (CGAL::to_double(point3d.z())!=floor) skip=true;
			}
		}
		if (skip) {
				out << "\n facet not on floor plane (z=" << floor <<"). skipping\n";
			out << " <!-- Halffacet visit end-->\n";
			std::cout << out.str();
			return;
		}
*/
		int contour_counter = 0;
		CGAL_forall_facet_cycles_of( i, hfacet ) {
			if ( i.is_shalfedge() ) {
				CGAL_Nef_polyhedron3::SHalfedge_around_facet_const_circulator c1(i), c2(c1);
				std::vector<CGAL_Nef_polyhedron2::Explorer::Point> contour;
				CGAL_For_all( c1, c2 ) {
					out << "around facet. c1 mark:" << c1->mark() << "\n";
					// c1->source() gives us an SVertex for the SHalfedge
					// c1->source()->target() gives us a Vertex??
					CGAL_Nef_polyhedron3::Point_3 point3d = c1->source()->target()->point();
					CGAL_Nef_polyhedron2::Explorer::Point point2d( point3d.x(), point3d.y() );
					out << "around facet. point3d:" << CGAL::to_double(point3d.x()) << "," << CGAL::to_double(point3d.y()) << "\n";;
					out << "around facet. point2d:" << CGAL::to_double(point2d.x()) << "," << CGAL::to_double(point2d.y()) << "\n";;
					if (contour.size()) out << "equality:" << (contour.back() == point2d) << "\n";;
					out << "equality2 :" << ( c1->target()->source() == c1->source()->target() ) << "\n";;
					contour.push_back( point2d );
				}

				// Type given to Polygon_2 has to match Nef2::Explorer::Point
				// (which is not the same as CGAL_Kernel2::Point)
				std::vector<CGAL_Nef_polyhedron2::Explorer::Point>::iterator xx;
				for ( xx=contour.begin(); xx!=contour.end(); ++xx ) {
					out << "pdump: " << CGAL::to_double(xx->x()) << "," << CGAL::to_double(xx->y()) << "\n";
				}
				out << "is simple 2:" << CGAL::is_simple_2( contour.begin(), contour.end() ) << "\n";
				//CGAL::Polygon_2<CGAL::Simple_cartesian<NT> > plainpoly2( contour.begin(), contour.end() );
				//out << "clockwise orientation: " << plainpoly2.is_clockwise_oriented() << "\n";
				tmpnef2d.reset( new CGAL_Nef_polyhedron2( contour.begin(), contour.end(), boundary ) );
				// *(tmpnef2d) = tmpnef2d->regularization();
				// mark here.

	      out << "\n<!-- ======== output accumulator 0: ==== -->\n";
				out << dump_cgal_nef_polyhedron2_svg( *output_nefpoly2d );

				if ( contour_counter == 0 ) {
					out << "\n <!-- contour is a body. make union(). " << contour.size() << " points. -->\n" ;
					*(output_nefpoly2d) += *(tmpnef2d);
				} else {
					*(output_nefpoly2d) *= *(tmpnef2d);
					if (debug) out << "\n<!-- contour is a hole. make intersection(). " << contour.size() << " points. -->\n";
				}

	      out << "\n<!-- ======== output tmp nef2d: ====== -->\n";
				out << dump_cgal_nef_polyhedron2_svg( *tmpnef2d );
	      out << "\n<!-- ======== output accumulator 1: ==== -->\n";
				out << dump_cgal_nef_polyhedron2_svg( *output_nefpoly2d );

				contour_counter++;
			} else {
				out << "trivial facet cycle skipped\n";
			}
		} // next facet cycle (i.e. next contour)
		out << " <!-- Halffacet visit end -->\n";
		std::cout << out.str();
	} // visit()
};




PolySetCGALEvaluator::PolySetCGALEvaluator(CGALEvaluator &cgalevaluator)
	: PolySetEvaluator(cgalevaluator.getTree()), cgalevaluator(cgalevaluator)
{
	this->debug = false;
}

PolySet *PolySetCGALEvaluator::evaluatePolySet(const ProjectionNode &node)
{
	// Before projecting, union all children
	CGAL_Nef_polyhedron sum;
	BOOST_FOREACH (AbstractNode * v, node.getChildren()) {
		if (v->modinst->isBackground()) continue;
		CGAL_Nef_polyhedron N = this->cgalevaluator.evaluateCGALMesh(*v);
		if (N.dim == 3) {
			if (sum.empty()) sum = N.copy();
			else sum += N;
		}
	}
	if (sum.empty()) return NULL;
	if (!sum.p3->is_simple()) {
		if (!node.cut_mode) {
			PRINT("WARNING: Body of projection(cut = false) isn't valid 2-manifold! Modify your design..");
			return new PolySet();
		}
	}

	// std::cout << sum.dump_svg() << std::flush; // input dump

	CGAL_Nef_polyhedron nef_poly;

	if (node.cut_mode) {
		CGAL::Failure_behaviour old_behaviour = CGAL::set_error_behaviour(CGAL::THROW_EXCEPTION);
		bool plane_intersect_fail = false;
		try {
			CGAL_Nef_polyhedron3::Plane_3 xy_plane = CGAL_Nef_polyhedron3::Plane_3( 0,0,1,0 );
			*sum.p3 = sum.p3->intersection( xy_plane, CGAL_Nef_polyhedron3::PLANE_ONLY);
		}
		catch (const CGAL::Failure_exception &e) {
			PRINTB("CGAL error in projection node during plane intersection: %s", e.what());
			plane_intersect_fail = true;
		}
 		if (plane_intersect_fail) {
			try {
				PRINT("Trying alternative intersection using very large thin box: ");
			  double inf = 1e8, eps = 0.001;
			  double x1 = -inf, x2 = +inf, y1 = -inf, y2 = +inf, z1 = -eps, z2 = eps;
				// dont use z of 0. there are bugs in CGAL.

				std::vector<Point_3> pts;
				pts.push_back( Point_3( x1, y1, z1 ) );
				pts.push_back( Point_3( x1, y2, z1 ) );
				pts.push_back( Point_3( x2, y2, z1 ) );
				pts.push_back( Point_3( x2, y1, z1 ) );
				pts.push_back( Point_3( x1, y1, z2 ) );
				pts.push_back( Point_3( x1, y2, z2 ) );
				pts.push_back( Point_3( x2, y2, z2 ) );
				pts.push_back( Point_3( x2, y1, z2 ) );

				CGAL_Polyhedron bigbox;
				CGAL::convex_hull_3( pts.begin(), pts.end(), bigbox );
				CGAL_Nef_polyhedron3 nef_bigbox( bigbox );
 				*sum.p3 = nef_bigbox.intersection( *sum.p3 );
			}
			catch (const CGAL::Failure_exception &e) {
				PRINTB("CGAL error in projection node during bigbox intersection: %s", e.what());
				// can we just return empty polyset?
				CGAL::set_error_behaviour(old_behaviour);
				return NULL;
			}
		}

		// remove z coordinates to make CGAL_Nef_polyhedron2
		std::cout << "<svg width=\"480px\" height=\"100000px\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">";
		try {
			Flattener2 flattener(true);
			CGAL_Nef_polyhedron3::Volume_const_iterator i;
			CGAL_Nef_polyhedron3::Shell_entry_const_iterator j;
			CGAL_Nef_polyhedron3::SFace_const_handle sface_handle;
			for ( i = sum.p3->volumes_begin(); i != sum.p3->volumes_end(); ++i ) {
				std::cout << "<!-- volume. mark: " << i->mark() << " -->\n";
				for ( j = i->shells_begin(); j != i->shells_end(); ++j ) {
					std::cout << "<!-- shell. mark: " << i->mark() << " -->\n";
//					if (i->mark()==1) {
						sface_handle = CGAL_Nef_polyhedron3::SFace_const_handle( j );
						sum.p3->visit_shell_objects( sface_handle , flattener );
//					}
					std::cout << "<!-- shell. end. -->\n";
				}
				std::cout << "<!-- volume end. -->\n";
			}
			//std::cout << flattener.out.str() << "\n";
			std::cout << "</svg>" << std::flush;

			//std::cout << "------- flattener dump \n" << flattener.dump() << "\n";
			nef_poly.p2 = flattener.output_nefpoly2d;
			nef_poly.dim = 2;
		}	catch (const CGAL::Failure_exception &e) {
			PRINTB("CGAL error in projection node while flattening: %s", e.what());
		}

		CGAL::set_error_behaviour(old_behaviour);

		//std::cout << sum.dump_svg() << std::flush; // cut dump
		//std::cout << nef_poly.dump_svg() << std::flush; // post-flattener dump

		//std::cout << "------- 2d output dump \n" << nef_poly.dump_svg() << "\n";
		// Extract polygons in the XY plane, ignoring all other polygons
		// FIXME: If the polyhedron is really thin, there might be unwanted polygons
		// in the XY plane, causing the resulting 2D polygon to be self-intersection
		// and cause a crash in CGALEvaluator::PolyReducer. The right solution is to
		// filter these polygons here. kintel 20120203.
		/*
		Grid2d<unsigned int> conversion_grid(GRID_COARSE);
		for (size_t i = 0; i < ps3->polygons.size(); i++) {
			for (size_t j = 0; j < ps3->polygons[i].size(); j++) {
				double x = ps3->polygons[i][j][0];
				double y = ps3->polygons[i][j][1];
				double z = ps3->polygons[i][j][2];
				if (z != 0)
					goto next_ps3_polygon_cut_mode;
				if (conversion_grid.align(x, y) == i+1)
					goto next_ps3_polygon_cut_mode;
				conversion_grid.data(x, y) = i+1;
			}
			ps->append_poly();
			for (size_t j = 0; j < ps3->polygons[i].size(); j++) {
				double x = ps3->polygons[i][j][0];
				double y = ps3->polygons[i][j][1];
				conversion_grid.align(x, y);
				ps->insert_vertex(x, y);
			}
		next_ps3_polygon_cut_mode:;
		}
		*/
	}
	// In projection mode all the triangles are projected manually into the XY plane
	else
	{
		PolySet *ps3 = sum.convertToPolyset();
		if (!ps3) return NULL;
		for (size_t i = 0; i < ps3->polygons.size(); i++)
		{
			int min_x_p = -1;
			double min_x_val = 0;
			for (size_t j = 0; j < ps3->polygons[i].size(); j++) {
				double x = ps3->polygons[i][j][0];
				if (min_x_p < 0 || x < min_x_val) {
					min_x_p = j;
					min_x_val = x;
				}
			}
			int min_x_p1 = (min_x_p+1) % ps3->polygons[i].size();
			int min_x_p2 = (min_x_p+ps3->polygons[i].size()-1) % ps3->polygons[i].size();
			double ax = ps3->polygons[i][min_x_p1][0] - ps3->polygons[i][min_x_p][0];
			double ay = ps3->polygons[i][min_x_p1][1] - ps3->polygons[i][min_x_p][1];
			double at = atan2(ay, ax);
			double bx = ps3->polygons[i][min_x_p2][0] - ps3->polygons[i][min_x_p][0];
			double by = ps3->polygons[i][min_x_p2][1] - ps3->polygons[i][min_x_p][1];
			double bt = atan2(by, bx);

			double eps = 0.000001;
			if (fabs(at - bt) < eps || (fabs(ax) < eps && fabs(ay) < eps) ||
					(fabs(bx) < eps && fabs(by) < eps)) {
				// this triangle is degenerated in projection
				continue;
			}

			std::list<CGAL_Nef_polyhedron2::Point> plist;
			for (size_t j = 0; j < ps3->polygons[i].size(); j++) {
				double x = ps3->polygons[i][j][0];
				double y = ps3->polygons[i][j][1];
				CGAL_Nef_polyhedron2::Point p = CGAL_Nef_polyhedron2::Point(x, y);
				if (at > bt)
					plist.push_front(p);
				else
					plist.push_back(p);
			}
			// FIXME: Should the CGAL_Nef_polyhedron2 be cached?
			if (nef_poly.empty()) {
				nef_poly.dim = 2;
				nef_poly.p2.reset(new CGAL_Nef_polyhedron2(plist.begin(), plist.end(), CGAL_Nef_polyhedron2::INCLUDED));
			}
			else {
				(*nef_poly.p2) += CGAL_Nef_polyhedron2(plist.begin(), plist.end(), CGAL_Nef_polyhedron2::INCLUDED);
			}
		}
		delete ps3;
	}

	PolySet *ps = nef_poly.convertToPolyset();
	assert( ps != NULL );
	ps->convexity = node.convexity;
	if (debug) std::cout << "--\n" << ps->dump() << "\n";

	return ps;
}

static void add_slice(PolySet *ps, const DxfData &dxf, DxfData::Path &path, double rot1, double rot2, double h1, double h2)
{
	bool splitfirst = sin(rot2 - rot1) >= 0.0;
	for (size_t j = 1; j < path.indices.size(); j++)
	{
		int k = j - 1;

		double jx1 = dxf.points[path.indices[j]][0] *  cos(rot1*M_PI/180) + dxf.points[path.indices[j]][1] * sin(rot1*M_PI/180);
		double jy1 = dxf.points[path.indices[j]][0] * -sin(rot1*M_PI/180) + dxf.points[path.indices[j]][1] * cos(rot1*M_PI/180);

		double jx2 = dxf.points[path.indices[j]][0] *  cos(rot2*M_PI/180) + dxf.points[path.indices[j]][1] * sin(rot2*M_PI/180);
		double jy2 = dxf.points[path.indices[j]][0] * -sin(rot2*M_PI/180) + dxf.points[path.indices[j]][1] * cos(rot2*M_PI/180);

		double kx1 = dxf.points[path.indices[k]][0] *  cos(rot1*M_PI/180) + dxf.points[path.indices[k]][1] * sin(rot1*M_PI/180);
		double ky1 = dxf.points[path.indices[k]][0] * -sin(rot1*M_PI/180) + dxf.points[path.indices[k]][1] * cos(rot1*M_PI/180);

		double kx2 = dxf.points[path.indices[k]][0] *  cos(rot2*M_PI/180) + dxf.points[path.indices[k]][1] * sin(rot2*M_PI/180);
		double ky2 = dxf.points[path.indices[k]][0] * -sin(rot2*M_PI/180) + dxf.points[path.indices[k]][1] * cos(rot2*M_PI/180);

		if (splitfirst)
		{
			ps->append_poly();
			if (path.is_inner) {
				ps->append_vertex(kx1, ky1, h1);
				ps->append_vertex(jx1, jy1, h1);
				ps->append_vertex(jx2, jy2, h2);
			} else {
				ps->insert_vertex(kx1, ky1, h1);
				ps->insert_vertex(jx1, jy1, h1);
				ps->insert_vertex(jx2, jy2, h2);
			}

			ps->append_poly();
			if (path.is_inner) {
				ps->append_vertex(kx2, ky2, h2);
				ps->append_vertex(kx1, ky1, h1);
				ps->append_vertex(jx2, jy2, h2);
			} else {
				ps->insert_vertex(kx2, ky2, h2);
				ps->insert_vertex(kx1, ky1, h1);
				ps->insert_vertex(jx2, jy2, h2);
			}
		}
		else
		{
			ps->append_poly();
			if (path.is_inner) {
				ps->append_vertex(kx1, ky1, h1);
				ps->append_vertex(jx1, jy1, h1);
				ps->append_vertex(kx2, ky2, h2);
			} else {
				ps->insert_vertex(kx1, ky1, h1);
				ps->insert_vertex(jx1, jy1, h1);
				ps->insert_vertex(kx2, ky2, h2);
			}

			ps->append_poly();
			if (path.is_inner) {
				ps->append_vertex(jx2, jy2, h2);
				ps->append_vertex(kx2, ky2, h2);
				ps->append_vertex(jx1, jy1, h1);
			} else {
				ps->insert_vertex(jx2, jy2, h2);
				ps->insert_vertex(kx2, ky2, h2);
				ps->insert_vertex(jx1, jy1, h1);
			}
		}
	}
}

PolySet *PolySetCGALEvaluator::evaluatePolySet(const LinearExtrudeNode &node)
{
	DxfData *dxf;

	if (node.filename.empty())
	{
		// Before extruding, union all (2D) children nodes
		// to a single DxfData, then tesselate this into a PolySet
		CGAL_Nef_polyhedron sum;
		BOOST_FOREACH (AbstractNode * v, node.getChildren()) {
			if (v->modinst->isBackground()) continue;
			CGAL_Nef_polyhedron N = this->cgalevaluator.evaluateCGALMesh(*v);
			if (!N.empty()) {
				if (N.dim != 2) {
					PRINT("ERROR: linear_extrude() is not defined for 3D child objects!");
				}
				else {
					if (sum.empty()) sum = N.copy();
					else sum += N;
				}
			}
		}

		if (sum.empty()) return NULL;
		dxf = sum.convertToDxfData();;
	} else {
		dxf = new DxfData(node.fn, node.fs, node.fa, node.filename, node.layername, node.origin_x, node.origin_y, node.scale);
	}

	PolySet *ps = extrudeDxfData(node, *dxf);
	delete dxf;
	return ps;
}

PolySet *PolySetCGALEvaluator::extrudeDxfData(const LinearExtrudeNode &node, DxfData &dxf)
{
	PolySet *ps = new PolySet();
	ps->convexity = node.convexity;

	double h1, h2;

	if (node.center) {
		h1 = -node.height/2.0;
		h2 = +node.height/2.0;
	} else {
		h1 = 0;
		h2 = node.height;
	}

	bool first_open_path = true;
	for (size_t i = 0; i < dxf.paths.size(); i++)
	{
		if (dxf.paths[i].is_closed)
			continue;
		if (first_open_path) {
			PRINTB("WARNING: Open paths in dxf_linear_extrude(file = \"%s\", layer = \"%s\"):",
					node.filename % node.layername);
			first_open_path = false;
		}
		PRINTB("   %9.5f %10.5f ... %10.5f %10.5f",
					 (dxf.points[dxf.paths[i].indices.front()][0] / node.scale + node.origin_x) %
					 (dxf.points[dxf.paths[i].indices.front()][1] / node.scale + node.origin_y) %
					 (dxf.points[dxf.paths[i].indices.back()][0] / node.scale + node.origin_x) %
					 (dxf.points[dxf.paths[i].indices.back()][1] / node.scale + node.origin_y));
	}


	if (node.has_twist)
	{
		dxf_tesselate(ps, dxf, 0, false, true, h1);
		dxf_tesselate(ps, dxf, node.twist, true, true, h2);
		for (int j = 0; j < node.slices; j++)
		{
			double t1 = node.twist*j / node.slices;
			double t2 = node.twist*(j+1) / node.slices;
			double g1 = h1 + (h2-h1)*j / node.slices;
			double g2 = h1 + (h2-h1)*(j+1) / node.slices;
			for (size_t i = 0; i < dxf.paths.size(); i++)
			{
				if (!dxf.paths[i].is_closed)
					continue;
				add_slice(ps, dxf, dxf.paths[i], t1, t2, g1, g2);
			}
		}
	}
	else
	{
		dxf_tesselate(ps, dxf, 0, false, true, h1);
		dxf_tesselate(ps, dxf, 0, true, true, h2);
		for (size_t i = 0; i < dxf.paths.size(); i++)
		{
			if (!dxf.paths[i].is_closed)
				continue;
			add_slice(ps, dxf, dxf.paths[i], 0, 0, h1, h2);
		}
	}

	return ps;
}

PolySet *PolySetCGALEvaluator::evaluatePolySet(const RotateExtrudeNode &node)
{
	DxfData *dxf;

	if (node.filename.empty())
	{
		// Before extruding, union all (2D) children nodes
		// to a single DxfData, then tesselate this into a PolySet
		CGAL_Nef_polyhedron sum;
		BOOST_FOREACH (AbstractNode * v, node.getChildren()) {
			if (v->modinst->isBackground()) continue;
			CGAL_Nef_polyhedron N = this->cgalevaluator.evaluateCGALMesh(*v);
			if (!N.empty()) {
				if (N.dim != 2) {
					PRINT("ERROR: rotate_extrude() is not defined for 3D child objects!");
				}
				else {
					if (sum.empty()) sum = N.copy();
					else sum += N;
				}
			}
		}

		if (sum.empty()) return NULL;
		dxf = sum.convertToDxfData();
	} else {
		dxf = new DxfData(node.fn, node.fs, node.fa, node.filename, node.layername, node.origin_x, node.origin_y, node.scale);
	}

	PolySet *ps = rotateDxfData(node, *dxf);
	delete dxf;
	return ps;
}

PolySet *PolySetCGALEvaluator::evaluatePolySet(const CgaladvNode &node)
{
	CGAL_Nef_polyhedron N = this->cgalevaluator.evaluateCGALMesh(node);
	PolySet *ps = NULL;
	if (!N.empty()) {
		ps = N.convertToPolyset();
		if (ps) ps->convexity = node.convexity;
	}

	return ps;
}

PolySet *PolySetCGALEvaluator::evaluatePolySet(const RenderNode &node)
{
	CGAL_Nef_polyhedron N = this->cgalevaluator.evaluateCGALMesh(node);
	PolySet *ps = NULL;
	if (!N.empty()) {
		if (N.dim == 3 && !N.p3->is_simple()) {
			PRINT("WARNING: Body of render() isn't valid 2-manifold!");
		}
		else {
			ps = N.convertToPolyset();
			if (ps) ps->convexity = node.convexity;
		}
	}
	return ps;
}

PolySet *PolySetCGALEvaluator::rotateDxfData(const RotateExtrudeNode &node, DxfData &dxf)
{
	PolySet *ps = new PolySet();
	ps->convexity = node.convexity;

	for (size_t i = 0; i < dxf.paths.size(); i++)
	{
		double max_x = 0;
		for (size_t j = 0; j < dxf.paths[i].indices.size(); j++) {
			max_x = fmax(max_x, dxf.points[dxf.paths[i].indices[j]][0]);
		}

		int fragments = get_fragments_from_r(max_x, node.fn, node.fs, node.fa);

		double ***points;
		points = new double**[fragments];
		for (int j=0; j < fragments; j++) {
			points[j] = new double*[dxf.paths[i].indices.size()];
			for (size_t k=0; k < dxf.paths[i].indices.size(); k++)
				points[j][k] = new double[3];
		}

		for (int j = 0; j < fragments; j++) {
			double a = (j*2*M_PI) / fragments - M_PI/2; // start on the X axis
			for (size_t k = 0; k < dxf.paths[i].indices.size(); k++) {
				points[j][k][0] = dxf.points[dxf.paths[i].indices[k]][0] * sin(a);
			 	points[j][k][1] = dxf.points[dxf.paths[i].indices[k]][0] * cos(a);
				points[j][k][2] = dxf.points[dxf.paths[i].indices[k]][1];
			}
		}

		for (int j = 0; j < fragments; j++) {
			int j1 = j + 1 < fragments ? j + 1 : 0;
			for (size_t k = 0; k < dxf.paths[i].indices.size(); k++) {
				int k1 = k + 1 < dxf.paths[i].indices.size() ? k + 1 : 0;
				if (points[j][k][0] != points[j1][k][0] ||
						points[j][k][1] != points[j1][k][1] ||
						points[j][k][2] != points[j1][k][2]) {
					ps->append_poly();
					ps->append_vertex(points[j ][k ][0],
							points[j ][k ][1], points[j ][k ][2]);
					ps->append_vertex(points[j1][k ][0],
							points[j1][k ][1], points[j1][k ][2]);
					ps->append_vertex(points[j ][k1][0],
							points[j ][k1][1], points[j ][k1][2]);
				}
				if (points[j][k1][0] != points[j1][k1][0] ||
						points[j][k1][1] != points[j1][k1][1] ||
						points[j][k1][2] != points[j1][k1][2]) {
					ps->append_poly();
					ps->append_vertex(points[j ][k1][0],
							points[j ][k1][1], points[j ][k1][2]);
					ps->append_vertex(points[j1][k ][0],
							points[j1][k ][1], points[j1][k ][2]);
					ps->append_vertex(points[j1][k1][0],
							points[j1][k1][1], points[j1][k1][2]);
				}
			}
		}

		for (int j=0; j < fragments; j++) {
			for (size_t k=0; k < dxf.paths[i].indices.size(); k++)
				delete[] points[j][k];
			delete[] points[j];
		}
		delete[] points;
	}
	
	return ps;
}
