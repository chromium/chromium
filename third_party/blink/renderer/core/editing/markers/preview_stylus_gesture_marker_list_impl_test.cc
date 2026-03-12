#include "third_party/blink/renderer/core/editing/markers/preview_stylus_gesture_marker_list_impl.h"

#include "third_party/blink/renderer/core/editing/markers/marker_test_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/preview_stylus_gesture_marker.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class PreviewStylusGestureMarkerListImplTest : public EditingTestBase {
 protected:
  PreviewStylusGestureMarkerListImplTest()
      : marker_list_(
            MakeGarbageCollected<PreviewStylusGestureMarkerListImpl>()) {}

  DocumentMarker* CreateMarker(unsigned start_offset, unsigned end_offset) {
    return MakeGarbageCollected<PreviewStylusGestureMarker>(
        start_offset, end_offset, Color::kBlack);
  }

  DocumentMarker* AddMarkers() {
    // Add some overlapping markers in an arbitrary order. Only the last one
    // will be kept.
    marker_list_->Add(CreateMarker(40, 50));
    marker_list_->Add(CreateMarker(10, 40));
    marker_list_->Add(CreateMarker(20, 50));
    marker_list_->Add(CreateMarker(10, 30));
    marker_list_->Add(CreateMarker(10, 50));
    marker_list_->Add(CreateMarker(30, 50));
    marker_list_->Add(CreateMarker(30, 40));
    marker_list_->Add(CreateMarker(10, 20));
    marker_list_->Add(CreateMarker(20, 40));
    DocumentMarker* last_marker = CreateMarker(20, 30);
    marker_list_->Add(last_marker);
    return last_marker;
  }

  Persistent<PreviewStylusGestureMarkerListImpl> marker_list_;
};

TEST_F(PreviewStylusGestureMarkerListImplTest, SingleMarkerAtATime) {
  DocumentMarker* last_marker = AddMarkers();
  DocumentMarkerVector markers = marker_list_->GetMarkers();

  EXPECT_EQ(1, markers.size());
  EXPECT_EQ(last_marker, markers[0]);
}

TEST_F(PreviewStylusGestureMarkerListImplTest, FirstMarkerIntersectingRange) {
  // marker has the range 20 to 30 as the last marker being added in AddMarkers
  // has the range 20-30.
  DocumentMarker* marker = AddMarkers();

  DocumentMarker* intersecting_marker =
      marker_list_->FirstMarkerIntersectingRange(31, 55);
  EXPECT_EQ(nullptr, intersecting_marker);

  DocumentMarker* intersecting_marker1 =
      marker_list_->FirstMarkerIntersectingRange(22, 28);
  EXPECT_EQ(marker, intersecting_marker1);

  DocumentMarker* intersecting_marker2 =
      marker_list_->FirstMarkerIntersectingRange(15, 25);
  EXPECT_EQ(marker, intersecting_marker2);

  DocumentMarker* intersecting_marker3 =
      marker_list_->FirstMarkerIntersectingRange(25, 35);
  EXPECT_EQ(marker, intersecting_marker3);

  DocumentMarker* intersecting_marker4 =
      marker_list_->FirstMarkerIntersectingRange(10, 40);
  EXPECT_EQ(marker, intersecting_marker4);

  DocumentMarker* intersecting_marker5 =
      marker_list_->FirstMarkerIntersectingRange(20, 30);
  EXPECT_EQ(marker, intersecting_marker5);
}

TEST_F(PreviewStylusGestureMarkerListImplTest, MarkersIntersectingRange) {
  HeapVector<Member<DocumentMarker>> expected_markers;
  DocumentMarker* temp_marker;
  temp_marker = AddMarkers();
  expected_markers.push_back(temp_marker);

  HeapVector<Member<DocumentMarker>> result_markers =
      marker_list_->MarkersIntersectingRange(31, 55);
  EXPECT_TRUE(result_markers.empty());

  HeapVector<Member<DocumentMarker>> result_markers1 =
      marker_list_->MarkersIntersectingRange(22, 28);
  EXPECT_EQ(expected_markers, result_markers1);

  HeapVector<Member<DocumentMarker>> result_markers2 =
      marker_list_->MarkersIntersectingRange(15, 25);
  EXPECT_EQ(expected_markers, result_markers2);

  HeapVector<Member<DocumentMarker>> result_markers3 =
      marker_list_->MarkersIntersectingRange(25, 35);
  EXPECT_EQ(expected_markers, result_markers3);

  HeapVector<Member<DocumentMarker>> result_markers4 =
      marker_list_->MarkersIntersectingRange(10, 40);
  EXPECT_EQ(expected_markers, result_markers4);

  HeapVector<Member<DocumentMarker>> result_markers5 =
      marker_list_->MarkersIntersectingRange(20, 30);
  EXPECT_EQ(expected_markers, result_markers5);
}
}  // namespace blink
