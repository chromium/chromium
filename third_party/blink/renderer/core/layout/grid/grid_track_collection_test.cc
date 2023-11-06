// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/grid/grid_track_collection.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

#define EXPECT_RANGE(expected_start, expected_count, range) \
  EXPECT_EQ(expected_start, range.start_line);              \
  EXPECT_EQ(expected_count, range.track_count);             \
  EXPECT_FALSE(range.IsCollapsed());
#define EXPECT_COLLAPSED_RANGE(expected_start, expected_count, range) \
  EXPECT_EQ(expected_start, range.start_line);                        \
  EXPECT_EQ(expected_count, range.track_count);                       \
  EXPECT_TRUE(range.IsCollapsed());
#define EXPECT_SET(expected_size, expected_count, iterator)                 \
  {                                                                         \
    GridSet expected_set(expected_count, expected_size,                     \
                         /* is_content_box_size_definite */ false);         \
    EXPECT_EQ(expected_set.track_size, iterator.CurrentSet().track_size);   \
    EXPECT_EQ(expected_set.track_count, iterator.CurrentSet().track_count); \
  }

}  // namespace

class GridTrackCollectionBaseTest : public GridTrackCollectionBase {
 public:
  struct TestTrackRange {
    wtf_size_t start_line;
    wtf_size_t track_count;
  };

  explicit GridTrackCollectionBaseTest(const Vector<wtf_size_t>& range_sizes)
      : GridTrackCollectionBase() {
    wtf_size_t start_line = 0;
    for (wtf_size_t size : range_sizes) {
      TestTrackRange range;
      range.start_line = start_line;
      range.track_count = size;
      ranges_.push_back(range);
      start_line += size;
    }
  }

  wtf_size_t RangeCount() const override { return ranges_.size(); }
  wtf_size_t RangeStartLine(wtf_size_t range_index) const override {
    return ranges_[range_index].start_line;
  }
  wtf_size_t RangeTrackCount(wtf_size_t range_index) const override {
    return ranges_[range_index].track_count;
  }

 private:
  Vector<TestTrackRange> ranges_;
};

class GridTrackCollectionTest : public RenderingTest {
 protected:
  GridRangeBuilder CreateRangeBuilder(const NGGridTrackList& explicit_tracks,
                                      const NGGridTrackList& implicit_tracks,
                                      wtf_size_t auto_repetitions) {
    return GridRangeBuilder(explicit_tracks, implicit_tracks, auto_repetitions);
  }

  Vector<GridTrackSize, 1> CreateTrackSizes(wtf_size_t track_count) {
    return {track_count, GridTrackSize(Length::Auto())};
  }

  void InitializeSetsForSizingCollection(
      const NGGridTrackList& explicit_tracks,
      const NGGridTrackList& implicit_tracks,
      GridSizingTrackCollection* sizing_collection) {
    sizing_collection->BuildSets(explicit_tracks, implicit_tracks);
    sizing_collection->InitializeSets();
  }

  const GridRangeVector& GetRangesFrom(
      const GridSizingTrackCollection& sizing_collection) {
    return sizing_collection.ranges_;
  }

  GridSizingTrackCollection::SetIterator IteratorForRange(
      GridSizingTrackCollection& track_collection,
      wtf_size_t range_index) {
    const wtf_size_t begin_set_index =
        track_collection.RangeBeginSetIndex(range_index);

    return track_collection.GetSetIterator(
        begin_set_index,
        begin_set_index + track_collection.RangeSetCount(range_index));
  }
};

TEST_F(GridTrackCollectionTest, TestRangeIndexFromGridLine) {
  // Small case.
  GridTrackCollectionBaseTest track_collection({3, 10u, 5u});
  EXPECT_EQ(0u, track_collection.RangeIndexFromGridLine(0u));
  EXPECT_EQ(1u, track_collection.RangeIndexFromGridLine(4u));
  EXPECT_EQ(2u, track_collection.RangeIndexFromGridLine(15u));

  // Small case with large repeat count.
  track_collection = GridTrackCollectionBaseTest({3000000u, 7u, 10u});
  EXPECT_EQ(0u, track_collection.RangeIndexFromGridLine(600u));
  EXPECT_EQ(1u, track_collection.RangeIndexFromGridLine(3000000u));
  EXPECT_EQ(1u, track_collection.RangeIndexFromGridLine(3000004u));

  // Larger case.
  track_collection = GridTrackCollectionBaseTest({
      10u,   // 0 - 9
      10u,   // 10 - 19
      10u,   // 20 - 29
      10u,   // 30 - 39
      20u,   // 40 - 59
      20u,   // 60 - 79
      20u,   // 80 - 99
      100u,  // 100 - 199
  });
  EXPECT_EQ(0u, track_collection.RangeIndexFromGridLine(0u));
  EXPECT_EQ(3u, track_collection.RangeIndexFromGridLine(35u));
  EXPECT_EQ(4u, track_collection.RangeIndexFromGridLine(40u));
  EXPECT_EQ(5u, track_collection.RangeIndexFromGridLine(79));
  EXPECT_EQ(7u, track_collection.RangeIndexFromGridLine(105u));
}

TEST_F(GridTrackCollectionTest, TestNGGridTrackList) {
  NGGridTrackList track_list;
  ASSERT_EQ(0u, track_list.RepeaterCount());
  EXPECT_FALSE(track_list.HasAutoRepeater());

  EXPECT_TRUE(track_list.AddRepeater(
      CreateTrackSizes(2), NGGridTrackRepeater::RepeatType::kInteger, 4));
  ASSERT_EQ(1u, track_list.RepeaterCount());
  EXPECT_EQ(8u, track_list.TrackCountWithoutAutoRepeat());
  EXPECT_EQ(4u, track_list.RepeatCount(0, 4));
  EXPECT_EQ(2u, track_list.RepeatSize(0));
  EXPECT_FALSE(track_list.HasAutoRepeater());

  // Can't add an empty repeater to a list.
  EXPECT_FALSE(track_list.AddRepeater(
      CreateTrackSizes(0), NGGridTrackRepeater::RepeatType::kAutoFit));
  EXPECT_FALSE(track_list.AddRepeater(
      CreateTrackSizes(3), NGGridTrackRepeater::RepeatType::kNoRepeat, 0));
  ASSERT_EQ(1u, track_list.RepeaterCount());

  EXPECT_TRUE(track_list.AddRepeater(CreateTrackSizes(1),
                                     NGGridTrackRepeater::RepeatType::kInteger,
                                     kNotFound - 20));
  ASSERT_EQ(2u, track_list.RepeaterCount());
  EXPECT_EQ(kNotFound - 12u, track_list.TrackCountWithoutAutoRepeat());
  EXPECT_EQ(kNotFound - 20u, track_list.RepeatCount(1, 4));
  EXPECT_EQ(1u, track_list.RepeatSize(1));
  EXPECT_FALSE(track_list.HasAutoRepeater());

  // Try to add a repeater that would overflow the total track count.
  EXPECT_FALSE(track_list.AddRepeater(
      CreateTrackSizes(5), NGGridTrackRepeater::RepeatType::kInteger, 7));
  EXPECT_FALSE(track_list.AddRepeater(
      CreateTrackSizes(7), NGGridTrackRepeater::RepeatType::kInteger, 5));
  EXPECT_FALSE(track_list.AddRepeater(
      CreateTrackSizes(31), NGGridTrackRepeater::RepeatType::kAutoFill));
  ASSERT_EQ(2u, track_list.RepeaterCount());

  EXPECT_TRUE(track_list.AddRepeater(
      CreateTrackSizes(3), NGGridTrackRepeater::RepeatType::kAutoFit));
  ASSERT_EQ(3u, track_list.RepeaterCount());
  EXPECT_EQ(kNotFound - 12u, track_list.TrackCountWithoutAutoRepeat());
  EXPECT_EQ(4u, track_list.RepeatCount(2, 4));
  EXPECT_EQ(3u, track_list.RepeatSize(2));
  EXPECT_TRUE(track_list.HasAutoRepeater());

  // Can't add more than one auto repeater to a list.
  EXPECT_FALSE(track_list.AddRepeater(
      CreateTrackSizes(3), NGGridTrackRepeater::RepeatType::kAutoFill));
  ASSERT_EQ(3u, track_list.RepeaterCount());
}

TEST_F(GridTrackCollectionTest, TestGridRangeBuilder) {
  NGGridTrackList explicit_tracks, implicit_tracks;
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      CreateTrackSizes(2), NGGridTrackRepeater::RepeatType::kInteger, 4));
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      CreateTrackSizes(3), NGGridTrackRepeater::RepeatType::kAutoFill));
  ASSERT_EQ(2u, explicit_tracks.RepeaterCount());

  auto range_builder = CreateRangeBuilder(explicit_tracks, implicit_tracks,
                                          /* auto_repetitions */ 3);
  const auto& ranges = range_builder.FinalizeRanges();

  EXPECT_EQ(2u, ranges.size());
  EXPECT_RANGE(0u, 8u, ranges[0]);
  EXPECT_RANGE(8u, 9u, ranges[1]);
}

TEST_F(GridTrackCollectionTest, TestGridRangeBuilderCollapsed) {
  NGGridTrackList explicit_tracks, implicit_tracks;
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      CreateTrackSizes(2), NGGridTrackRepeater::RepeatType::kInteger, 4));
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      CreateTrackSizes(3), NGGridTrackRepeater::RepeatType::kAutoFit));
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      CreateTrackSizes(3), NGGridTrackRepeater::RepeatType::kInteger, 7));
  ASSERT_EQ(3u, explicit_tracks.RepeaterCount());

  auto range_builder = CreateRangeBuilder(explicit_tracks, implicit_tracks,
                                          /* auto_repetitions */ 3);
  const auto& ranges = range_builder.FinalizeRanges();

  EXPECT_EQ(3u, ranges.size());
  EXPECT_RANGE(0u, 8u, ranges[0]);
  EXPECT_COLLAPSED_RANGE(8u, 9u, ranges[1]);
  EXPECT_RANGE(17u, 21u, ranges[2]);
}

TEST_F(GridTrackCollectionTest, TestGridRangeBuilderImplicit) {
  NGGridTrackList explicit_tracks;
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      CreateTrackSizes(2), NGGridTrackRepeater::RepeatType::kInteger, 4));
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      CreateTrackSizes(3), NGGridTrackRepeater::RepeatType::kInteger, 3));
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      CreateTrackSizes(3), NGGridTrackRepeater::RepeatType::kInteger, 7));
  ASSERT_EQ(3u, explicit_tracks.RepeaterCount());

  NGGridTrackList implicit_tracks;
  ASSERT_TRUE(implicit_tracks.AddRepeater(
      CreateTrackSizes(8), NGGridTrackRepeater::RepeatType::kInteger, 2));

  auto range_builder = CreateRangeBuilder(explicit_tracks, implicit_tracks,
                                          /* auto_repetitions */ 3);

  wtf_size_t range1_start, range1_end, range2_start, range2_end;
  range_builder.EnsureTrackCoverage(3, 40, &range1_start, &range1_end);
  range_builder.EnsureTrackCoverage(3, 40, &range2_start, &range2_end);
  const auto& ranges = range_builder.FinalizeRanges();

  EXPECT_EQ(1u, range1_start);
  EXPECT_EQ(4u, range1_end);
  EXPECT_EQ(1u, range2_start);
  EXPECT_EQ(4u, range2_end);

  EXPECT_EQ(5u, ranges.size());
  EXPECT_RANGE(0u, 3u, ranges[0]);
  EXPECT_FALSE(ranges[0].IsImplicit());
  EXPECT_RANGE(3u, 5u, ranges[1]);
  EXPECT_FALSE(ranges[1].IsImplicit());
  EXPECT_RANGE(8u, 9u, ranges[2]);
  EXPECT_FALSE(ranges[2].IsImplicit());
  EXPECT_RANGE(17u, 21u, ranges[3]);
  EXPECT_FALSE(ranges[3].IsImplicit());
  EXPECT_RANGE(38u, 5u, ranges[4]);
  EXPECT_TRUE(ranges[4].IsImplicit());
}

TEST_F(GridTrackCollectionTest, TestGridSetTrackSizeNormalization) {
  auto TestTrackSizeNormalization =
      [](const GridTrackSize& track_definition,
         const GridTrackSize& expected_track_size_in_definite_container,
         const GridTrackSize& expected_track_size_in_indefinite_container) {
        GridSet normalized_set(/* track_count */ 1, track_definition,
                               /* is_content_box_size_indefinite */ false);
        EXPECT_EQ(expected_track_size_in_definite_container,
                  normalized_set.track_size);

        normalized_set = GridSet(/* track_count */ 1, track_definition,
                                 /* is_content_box_size_indefinite */ true);
        EXPECT_EQ(expected_track_size_in_indefinite_container,
                  normalized_set.track_size);
      };

  // auto
  TestTrackSizeNormalization(GridTrackSize(Length::Auto()),
                             GridTrackSize(Length::Auto(), Length::Auto()),
                             GridTrackSize(Length::Auto(), Length::Auto()));
  // 10%
  TestTrackSizeNormalization(
      GridTrackSize(Length::Percent(10)),
      GridTrackSize(Length::Percent(10), Length::Percent(10)),
      GridTrackSize(Length::Auto(), Length::Auto()));
  // minmax(50px, 20%)
  TestTrackSizeNormalization(
      GridTrackSize(Length::Fixed(50), Length::Percent(20)),
      GridTrackSize(Length::Fixed(50), Length::Percent(20)),
      GridTrackSize(Length::Fixed(50), Length::Auto()));
  // min-content
  TestTrackSizeNormalization(
      GridTrackSize(Length::MinContent()),
      GridTrackSize(Length::MinContent(), Length::MinContent()),
      GridTrackSize(Length::MinContent(), Length::MinContent()));
  // max-content
  TestTrackSizeNormalization(
      GridTrackSize(Length::MaxContent()),
      GridTrackSize(Length::MaxContent(), Length::MaxContent()),
      GridTrackSize(Length::MaxContent(), Length::MaxContent()));
  // minmax(1fr, 3fr)
  TestTrackSizeNormalization(
      GridTrackSize(Length::Flex(1.0), Length::Flex(3.0)),
      GridTrackSize(Length::Auto(), Length::Flex(3.0)),
      GridTrackSize(Length::Auto(), Length::Flex(3.0)));
  // fit-content(40%)
  TestTrackSizeNormalization(
      GridTrackSize(Length::Percent(40), kFitContentTrackSizing),
      GridTrackSize(Length::Percent(40), kFitContentTrackSizing),
      GridTrackSize(Length::Auto(), Length::MaxContent()));
}

TEST_F(GridTrackCollectionTest, TestGridSizingTrackCollectionSetIterator) {
  Vector<wtf_size_t> set_counts = {2, 5, 3, 11, 13, 7};

  wtf_size_t expected_set_count = 0;
  NGGridTrackList explicit_tracks, implicit_tracks;
  for (wtf_size_t set_count : set_counts) {
    Vector<GridTrackSize, 1> track_sizes;
    for (wtf_size_t i = 0; i < set_count; ++i)
      track_sizes.emplace_back(Length::Flex(expected_set_count++));
    ASSERT_TRUE(explicit_tracks.AddRepeater(
        track_sizes, NGGridTrackRepeater::RepeatType::kNoRepeat, 1));
  }
  ASSERT_EQ(set_counts.size(), explicit_tracks.RepeaterCount());

  auto range_builder = CreateRangeBuilder(explicit_tracks, implicit_tracks,
                                          /* auto_repetitions */ 0);

  GridSizingTrackCollection track_collection(range_builder.FinalizeRanges());
  InitializeSetsForSizingCollection(explicit_tracks, implicit_tracks,
                                    &track_collection);
  const auto& ranges = GetRangesFrom(track_collection);

  // Test the set iterator for the entire collection.
  wtf_size_t set_count = 0;
  for (auto set_iterator = track_collection.GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    EXPECT_SET(GridTrackSize(Length::Flex(set_count++)), 1u, set_iterator);
  }
  EXPECT_EQ(expected_set_count, set_count);

  // For each range in the collection, test its respective set iterator.
  set_count = 0;
  wtf_size_t range_count = 0;
  for (wtf_size_t i = 0; i < ranges.size(); ++i) {
    EXPECT_RANGE(set_count, set_counts[range_count], ranges[i]);

    wtf_size_t current_range_set_count = 0;
    for (auto set_iterator = IteratorForRange(track_collection, i);
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      EXPECT_SET(GridTrackSize(Length::Flex(set_count++)), 1u, set_iterator);
      ++current_range_set_count;
    }
    EXPECT_EQ(set_counts[range_count++], current_range_set_count);
  }

  EXPECT_EQ(set_counts.size(), range_count);
  EXPECT_EQ(expected_set_count, set_count);
}

TEST_F(GridTrackCollectionTest, TestGridSizingTrackCollectionExplicitTracks) {
  // We'll use fixed size tracks to differentiate between them by int value.
  NGGridTrackList explicit_tracks, implicit_tracks;

  // repeat(3, 1px 2px 3px)
  Vector<GridTrackSize, 1> track_sizes = {GridTrackSize(Length::Fixed(1)),
                                          GridTrackSize(Length::Fixed(2)),
                                          GridTrackSize(Length::Fixed(3))};
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      track_sizes, NGGridTrackRepeater::RepeatType::kInteger, 3));

  // repeat(auto-fit, 4px 5px)
  track_sizes = {GridTrackSize(Length::Fixed(4)),
                 GridTrackSize(Length::Fixed(5))};
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      track_sizes, NGGridTrackRepeater::RepeatType::kAutoFit));
  ASSERT_EQ(2u, explicit_tracks.RepeaterCount());

  auto range_builder = CreateRangeBuilder(explicit_tracks, implicit_tracks,
                                          /* auto_repetitions */ 5);

  wtf_size_t range1_start, range1_end, range2_start, range2_end, range3_start,
      range3_end, range4_start, range4_end;
  range_builder.EnsureTrackCoverage(2, 4, &range1_start, &range1_end);
  range_builder.EnsureTrackCoverage(12, 4, &range2_start, &range2_end);
  range_builder.EnsureTrackCoverage(17, 3, &range3_start, &range3_end);
  range_builder.EnsureTrackCoverage(22, 5, &range4_start, &range4_end);

  GridSizingTrackCollection track_collection(range_builder.FinalizeRanges());
  InitializeSetsForSizingCollection(explicit_tracks, implicit_tracks,
                                    &track_collection);
  const auto& ranges = GetRangesFrom(track_collection);

  EXPECT_EQ(1u, range1_start);
  EXPECT_EQ(1u, range1_end);
  EXPECT_EQ(4u, range2_start);
  EXPECT_EQ(4u, range2_end);
  EXPECT_EQ(6u, range3_start);
  EXPECT_EQ(7u, range3_end);
  EXPECT_EQ(9u, range4_start);
  EXPECT_EQ(9u, range4_end);

  EXPECT_EQ(10u, ranges.size());
  EXPECT_RANGE(0u, 2u, ranges[0]);
  auto set_iterator = IteratorForRange(track_collection, /* range_index */ 0);
  EXPECT_SET(GridTrackSize(Length::Fixed(1)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(2)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_RANGE(2u, 4u, ranges[1]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 1);
  EXPECT_SET(GridTrackSize(Length::Fixed(3)), 2u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(1)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(2)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_RANGE(6u, 3u, ranges[2]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 2);
  EXPECT_SET(GridTrackSize(Length::Fixed(1)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(2)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(3)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_COLLAPSED_RANGE(9u, 3u, ranges[3]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 3);
  EXPECT_TRUE(set_iterator.IsAtEnd());

  EXPECT_RANGE(12u, 4u, ranges[4]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 4);
  EXPECT_SET(GridTrackSize(Length::Fixed(5)), 2u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(4)), 2u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_COLLAPSED_RANGE(16u, 1u, ranges[5]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 5);
  EXPECT_TRUE(set_iterator.IsAtEnd());

  EXPECT_RANGE(17u, 2u, ranges[6]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 6);
  EXPECT_SET(GridTrackSize(Length::Fixed(4)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(5)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_RANGE(19u, 1u, ranges[7]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 7);
  EXPECT_SET(GridTrackSize(Length::Auto()), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_RANGE(20u, 2u, ranges[8]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 8);
  EXPECT_SET(GridTrackSize(Length::Auto()), 2u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_RANGE(22u, 5u, ranges[9]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 9);
  EXPECT_SET(GridTrackSize(Length::Auto()), 5u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());
}

TEST_F(GridTrackCollectionTest, TestGridSizingTrackCollectionImplicitTracks) {
  // We'll use fixed size tracks to differentiate between them by int value.
  NGGridTrackList explicit_tracks, implicit_tracks;

  // Explicit grid: 1px 2px 3px 4px
  Vector<GridTrackSize, 1> track_sizes = {
      GridTrackSize(Length::Fixed(1)), GridTrackSize(Length::Fixed(2)),
      GridTrackSize(Length::Fixed(3)), GridTrackSize(Length::Fixed(4))};
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      track_sizes, NGGridTrackRepeater::RepeatType::kNoRepeat, 1));
  ASSERT_EQ(1u, explicit_tracks.RepeaterCount());

  // Implicit grid: 5px 6px 7px
  track_sizes = {GridTrackSize(Length::Fixed(5)),
                 GridTrackSize(Length::Fixed(6)),
                 GridTrackSize(Length::Fixed(7))};
  ASSERT_TRUE(implicit_tracks.AddRepeater(
      track_sizes, NGGridTrackRepeater::RepeatType::kNoRepeat, 1));
  ASSERT_EQ(1u, implicit_tracks.RepeaterCount());

  auto range_builder = CreateRangeBuilder(explicit_tracks, implicit_tracks,
                                          /* auto_repetitions */ 0);

  wtf_size_t range1_start, range1_end, range2_start, range2_end;
  range_builder.EnsureTrackCoverage(2, 13, &range1_start, &range1_end);
  range_builder.EnsureTrackCoverage(23, 2, &range2_start, &range2_end);

  GridSizingTrackCollection track_collection(range_builder.FinalizeRanges());
  InitializeSetsForSizingCollection(explicit_tracks, implicit_tracks,
                                    &track_collection);
  const auto& ranges = GetRangesFrom(track_collection);

  EXPECT_EQ(1u, range1_start);
  EXPECT_EQ(2u, range1_end);
  EXPECT_EQ(4u, range2_start);
  EXPECT_EQ(4u, range2_end);

  EXPECT_EQ(5u, ranges.size());
  EXPECT_RANGE(0u, 2u, ranges[0]);
  auto set_iterator = IteratorForRange(track_collection, /* range_index */ 0);
  EXPECT_SET(GridTrackSize(Length::Fixed(1)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(2)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_RANGE(2u, 2u, ranges[1]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 1);
  EXPECT_SET(GridTrackSize(Length::Fixed(3)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(4)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_RANGE(4u, 11u, ranges[2]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 2);
  EXPECT_SET(GridTrackSize(Length::Fixed(5)), 4u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(6)), 4u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(7)), 3u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_RANGE(15u, 8u, ranges[3]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 3);
  EXPECT_SET(GridTrackSize(Length::Fixed(7)), 3u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(5)), 3u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(6)), 2u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());

  EXPECT_RANGE(23u, 2u, ranges[4]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 4);
  EXPECT_SET(GridTrackSize(Length::Fixed(6)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(7)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());
}

TEST_F(GridTrackCollectionTest,
       TestGridSizingTrackCollectionIntrinsicAndFlexTracks) {
  // Test that the ranges of a |GridSizingTrackCollection| correctly
  // cache if they contain intrinsic or flexible tracks.
  NGGridTrackList explicit_tracks, implicit_tracks;

  // repeat(2, min-content 1fr 2px 3px)
  Vector<GridTrackSize, 1> track_sizes = {
      GridTrackSize(Length::MinContent()), GridTrackSize(Length::Flex(1.0)),
      GridTrackSize(Length::Fixed(2)), GridTrackSize(Length::Fixed(3))};
  ASSERT_TRUE(explicit_tracks.AddRepeater(
      track_sizes, NGGridTrackRepeater::RepeatType::kInteger, 2));
  ASSERT_EQ(1u, explicit_tracks.RepeaterCount());

  auto range_builder = CreateRangeBuilder(explicit_tracks, implicit_tracks,
                                          /* auto_repetitions */ 0);

  wtf_size_t range1_start, range1_end, range2_start, range2_end;
  range_builder.EnsureTrackCoverage(1, 2, &range1_start, &range1_end);
  range_builder.EnsureTrackCoverage(7, 4, &range2_start, &range2_end);

  GridSizingTrackCollection track_collection(range_builder.FinalizeRanges());
  InitializeSetsForSizingCollection(explicit_tracks, implicit_tracks,
                                    &track_collection);
  const auto& ranges = GetRangesFrom(track_collection);

  EXPECT_EQ(1u, range1_start);
  EXPECT_EQ(1u, range1_end);
  EXPECT_EQ(3u, range2_start);
  EXPECT_EQ(4u, range2_end);

  EXPECT_EQ(5u, ranges.size());
  EXPECT_RANGE(0u, 1u, ranges[0]);
  auto set_iterator = IteratorForRange(track_collection, /* range_index */ 0);
  EXPECT_SET(GridTrackSize(Length::MinContent()), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());
  EXPECT_FALSE(
      ranges[0].properties.HasProperty(TrackSpanProperties::kHasFlexibleTrack));
  EXPECT_TRUE(ranges[0].properties.HasProperty(
      TrackSpanProperties::kHasIntrinsicTrack));

  EXPECT_RANGE(1u, 2u, ranges[1]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 1);
  EXPECT_SET(GridTrackSize(Length::Flex(1.0)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(2)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());
  EXPECT_TRUE(
      ranges[1].properties.HasProperty(TrackSpanProperties::kHasFlexibleTrack));
  EXPECT_TRUE(ranges[1].properties.HasProperty(
      TrackSpanProperties::kHasIntrinsicTrack));

  EXPECT_RANGE(3u, 4u, ranges[2]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 2);
  EXPECT_SET(GridTrackSize(Length::Fixed(3)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::MinContent()), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Flex(1.0)), 1u, set_iterator);
  EXPECT_TRUE(set_iterator.MoveToNextSet());
  EXPECT_SET(GridTrackSize(Length::Fixed(2)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());
  EXPECT_TRUE(
      ranges[2].properties.HasProperty(TrackSpanProperties::kHasFlexibleTrack));
  EXPECT_TRUE(ranges[2].properties.HasProperty(
      TrackSpanProperties::kHasIntrinsicTrack));

  EXPECT_RANGE(7u, 1u, ranges[3]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 3);
  EXPECT_SET(GridTrackSize(Length::Fixed(3)), 1u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());
  EXPECT_FALSE(
      ranges[3].properties.HasProperty(TrackSpanProperties::kHasFlexibleTrack));
  EXPECT_FALSE(ranges[3].properties.HasProperty(
      TrackSpanProperties::kHasIntrinsicTrack));

  EXPECT_RANGE(8u, 3u, ranges[4]);
  set_iterator = IteratorForRange(track_collection, /* range_index */ 4);
  EXPECT_SET(GridTrackSize(Length::Auto()), 3u, set_iterator);
  EXPECT_FALSE(set_iterator.MoveToNextSet());
  EXPECT_FALSE(
      ranges[4].properties.HasProperty(TrackSpanProperties::kHasFlexibleTrack));
  EXPECT_TRUE(ranges[4].properties.HasProperty(
      TrackSpanProperties::kHasIntrinsicTrack));
}

}  // namespace blink
