// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"

namespace blink {

class MasonryLayoutAlgorithmTest : public BaseLayoutAlgorithmTest {
 protected:
  void SetUp() override { BaseLayoutAlgorithmTest::SetUp(); }

  void ComputeCrossAxisTrackSizes(const MasonryLayoutAlgorithm& algorithm) {
    cross_axis_tracks_ = std::make_unique<GridSizingTrackCollection>(
        algorithm.ComputeCrossAxisTrackSizes());
  }

  const GridRangeVector& Ranges() const { return cross_axis_tracks_->ranges_; }

  std::unique_ptr<GridSizingTrackCollection> cross_axis_tracks_;
};

TEST_F(MasonryLayoutAlgorithmTest, TemplateTracksExpandedRanges) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #masonry {
      display: masonry;
      masonry-template-tracks: 5% repeat(3, 10px auto) repeat(1, auto 5px 1fr);
    }
    </style>
    <div id="masonry"></div>
  )HTML");

  BlockNode node(GetLayoutBoxByElementId("masonry"));

  const auto space = ConstructBlockLayoutTestConstraintSpace(
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      LogicalSize(LayoutUnit(100), LayoutUnit(100)),
      /*stretch_inline_size_if_auto=*/true,
      /*is_new_formatting_context=*/true);

  const auto fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /*break_token=*/nullptr);

  MasonryLayoutAlgorithm algorithm({node, fragment_geometry, space});
  ComputeCrossAxisTrackSizes(algorithm);

  const auto& ranges = Ranges();
  EXPECT_EQ(ranges.size(), 10u);

  const Vector<wtf_size_t> expected_repeater_indices = {0, 1, 1, 1, 1,
                                                        1, 1, 2, 2, 2};
  const Vector<wtf_size_t> expected_repeater_offsets = {0, 0, 1, 0, 1,
                                                        0, 1, 0, 1, 2};

  for (wtf_size_t i = 0; i < ranges.size(); ++i) {
    EXPECT_EQ(ranges[i].begin_set_index, i);
    EXPECT_EQ(ranges[i].repeater_index, expected_repeater_indices[i]);
    EXPECT_EQ(ranges[i].repeater_offset, expected_repeater_offsets[i]);
    EXPECT_EQ(ranges[i].set_count, 1u);
    EXPECT_EQ(ranges[i].start_line, i);
    EXPECT_EQ(ranges[i].track_count, 1u);
  }
}

}  // namespace blink
