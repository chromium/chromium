// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/content/capture_resolution_chooser.h"

#include <stddef.h>

#include <numeric>

#include "base/location.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using base::Location;

namespace media {

namespace {

// 16:9 maximum and minimum frame sizes.
constexpr int kMaxFrameWidth = 3840;
constexpr int kMaxFrameHeight = 2160;
constexpr int kMinFrameWidth = 320;
constexpr int kMinFrameHeight = 180;

// Smallest non-empty size for I420 video.
constexpr int kSmallestNonEmptyWidth = 2;
constexpr int kSmallestNonEmptyHeight = 2;

// Checks whether |size| is strictly between (inclusive) |min_size| and
// |max_size| and has the same aspect ratio as |max_size|.
void ExpectIsWithinBoundsAndSameAspectRatio(const Location& location,
                                            const gfx::Size& min_size,
                                            const gfx::Size& max_size,
                                            const gfx::Size& size) {
  SCOPED_TRACE(::testing::Message() << "From here: " << location.ToString());
  EXPECT_LE(min_size.width(), size.width());
  EXPECT_LE(min_size.height(), size.height());
  EXPECT_GE(max_size.width(), size.width());
  EXPECT_GE(max_size.height(), size.height());
  EXPECT_NEAR(static_cast<double>(max_size.width()) / max_size.height(),
              static_cast<double>(size.width()) / size.height(), 0.01);
}

// Test that the correct snapped frame sizes are computed for a |chooser|
// configured with either of the variable-resolution change policies, and are
// correctly found when searched.
void TestSnappedFrameSizes(CaptureResolutionChooser* chooser,
                           const gfx::Size& smallest_size) {
  const int kSizes[17][2] = {
      {kMaxFrameWidth, kMaxFrameHeight},
      {3520, 1980},
      {3200, 1800},
      {2880, 1620},
      {2560, 1440},
      {2240, 1260},
      {1920, 1080},
      {1760, 990},
      {1600, 900},
      {1440, 810},
      {1280, 720},
      {1120, 630},
      {960, 540},
      {800, 450},
      {640, 360},
      {480, 270},
      {320, 180},
  };

  const gfx::Size largest_size(kMaxFrameWidth, kMaxFrameHeight);
  chooser->SetSourceSize(largest_size);

  // There should be no size larger than the largest size.
  for (int i = 1; i < 4; ++i) {
    EXPECT_EQ(largest_size,
              chooser->FindLargerFrameSize(largest_size.GetArea(), i));
    EXPECT_EQ(largest_size,
              chooser->FindLargerFrameSize(largest_size.GetArea() * 2, +i));
  }

  // There should be no size smaller than the smallest size.
  for (int i = 1; i < 4; ++i) {
    EXPECT_EQ(smallest_size,
              chooser->FindSmallerFrameSize(smallest_size.GetArea(), i));
    EXPECT_EQ(smallest_size,
              chooser->FindSmallerFrameSize(smallest_size.GetArea() / 2, i));
  }

  // Test the "find Nth lower size" logic.
  for (size_t skips = 1; skips < 4; ++skips) {
    for (size_t i = skips; i < std::size(kSizes); ++i) {
      EXPECT_EQ(
          gfx::Size(kSizes[i][0], kSizes[i][1]),
          chooser->FindSmallerFrameSize(
              gfx::Size(kSizes[i - skips][0], kSizes[i - skips][1]).GetArea(),
              skips));
    }
  }

  // Test the "find Nth higher size" logic.
  for (size_t skips = 1; skips < 4; ++skips) {
    for (size_t i = skips; i < std::size(kSizes); ++i) {
      EXPECT_EQ(gfx::Size(kSizes[i - skips][0], kSizes[i - skips][1]),
                chooser->FindLargerFrameSize(
                    gfx::Size(kSizes[i][0], kSizes[i][1]).GetArea(), skips));
    }
  }

  // Test the "find nearest size" logic.
  for (size_t i = 1; i < std::size(kSizes) - 1; ++i) {
    const gfx::Size size(kSizes[i][0], kSizes[i][1]);
    const int a_somewhat_smaller_area =
        gfx::Size((kSizes[i - 1][0] + 3 * kSizes[i][0]) / 4,
                  (kSizes[i - 1][1] + 3 * kSizes[i][1]) / 4).GetArea();
    EXPECT_EQ(size, chooser->FindNearestFrameSize(a_somewhat_smaller_area));

    const int a_smidge_smaller_area = size.GetArea() - 1;
    EXPECT_EQ(size, chooser->FindNearestFrameSize(a_smidge_smaller_area));

    const int a_smidge_larger_area = size.GetArea() + 1;
    EXPECT_EQ(size, chooser->FindNearestFrameSize(a_smidge_larger_area));

    const int a_somewhat_larger_area =
        gfx::Size((kSizes[i + 1][0] + 3 * kSizes[i][0]) / 4,
                  (kSizes[i + 1][1] + 3 * kSizes[i][1]) / 4).GetArea();
    EXPECT_EQ(size, chooser->FindNearestFrameSize(a_somewhat_larger_area));
  }
}

// Test that setting the target frame area results in the correct capture sizes
// being computed for a |chooser| configured with either of the
// variable-resolution change policies.
void TestTargetedFrameAreas(CaptureResolutionChooser* chooser,
                            const gfx::Size& smallest_size) {
  chooser->SetSourceSize(gfx::Size(1280, 720));

  // The computed capture size cannot be larger than the source size, even
  // though the |chooser| is configured with a larger max frame size.
  chooser->SetTargetFrameArea(kMaxFrameWidth * kMaxFrameHeight);
  EXPECT_EQ(gfx::Size(1280, 720), chooser->capture_size());

  chooser->SetTargetFrameArea(1280 * 720 + 1);
  EXPECT_EQ(gfx::Size(1280, 720), chooser->capture_size());
  chooser->SetTargetFrameArea(1280 * 720 - 1);
  EXPECT_EQ(gfx::Size(1280, 720), chooser->capture_size());

  chooser->SetTargetFrameArea(1120 * 630 + 1);
  EXPECT_EQ(gfx::Size(1120, 630), chooser->capture_size());
  chooser->SetTargetFrameArea(1120 * 630 - 1);
  EXPECT_EQ(gfx::Size(1120, 630), chooser->capture_size());

  chooser->SetTargetFrameArea(800 * 450 + 1);
  EXPECT_EQ(gfx::Size(800, 450), chooser->capture_size());
  chooser->SetTargetFrameArea(800 * 450 - 1);
  EXPECT_EQ(gfx::Size(800, 450), chooser->capture_size());

  chooser->SetTargetFrameArea(640 * 360 + 1);
  EXPECT_EQ(gfx::Size(640, 360), chooser->capture_size());
  chooser->SetTargetFrameArea(640 * 360 - 1);
  EXPECT_EQ(gfx::Size(640, 360), chooser->capture_size());

  chooser->SetTargetFrameArea(smallest_size.GetArea() + 1);
  EXPECT_EQ(smallest_size, chooser->capture_size());
  chooser->SetTargetFrameArea(smallest_size.GetArea() - 1);
  EXPECT_EQ(smallest_size, chooser->capture_size());

  chooser->SetTargetFrameArea(smallest_size.GetArea() / 2);
  EXPECT_EQ(smallest_size, chooser->capture_size());

  chooser->SetTargetFrameArea(0);
  EXPECT_EQ(smallest_size, chooser->capture_size());

  // If the source size has increased, the |chooser| is now permitted to compute
  // higher capture sizes.
  chooser->SetSourceSize(gfx::Size(kMaxFrameWidth, kMaxFrameHeight));
  chooser->SetTargetFrameArea(kMaxFrameWidth * kMaxFrameHeight);
  EXPECT_EQ(gfx::Size(kMaxFrameWidth, kMaxFrameHeight),
            chooser->capture_size());

  chooser->SetTargetFrameArea(3200 * 1800 + 1);
  EXPECT_EQ(gfx::Size(3200, 1800), chooser->capture_size());
  chooser->SetTargetFrameArea(3200 * 1800 - 1);
  EXPECT_EQ(gfx::Size(3200, 1800), chooser->capture_size());

  chooser->SetTargetFrameArea(640 * 360 + 1);
  EXPECT_EQ(gfx::Size(640, 360), chooser->capture_size());
  chooser->SetTargetFrameArea(640 * 360 - 1);
  EXPECT_EQ(gfx::Size(640, 360), chooser->capture_size());

  chooser->SetTargetFrameArea(0);
  EXPECT_EQ(smallest_size, chooser->capture_size());
}

// Determines that there is only one snapped frame size by implication: This
// function searches for the nearest/larger/smaller frame sizes around
// |the_one_size| and checks that there is only ever one result.
void ExpectOnlyOneSnappedFrameSize(const CaptureResolutionChooser& chooser,
                                   const gfx::Size& the_one_size) {
  for (int i = 1; i < 4; ++i) {
    EXPECT_EQ(the_one_size,
              chooser.FindNearestFrameSize(the_one_size.GetArea() * i));
    EXPECT_EQ(the_one_size,
              chooser.FindSmallerFrameSize(the_one_size.GetArea(), i));
    EXPECT_EQ(the_one_size,
              chooser.FindLargerFrameSize(the_one_size.GetArea(), i));
  }
}

}  // namespace

// While clients should always strive to set their own constraints before
// querying the CaptureResolutionChooser, do test that the reasonable "default"
// behavior is exhibited beforehand.
TEST(CaptureResolutionChooserTest, DefaultCaptureSizeIfNeverSetConstraints) {
  CaptureResolutionChooser chooser;
  EXPECT_EQ(CaptureResolutionChooser::kDefaultCaptureSize,
            chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMinFrameWidth, kMinFrameHeight));
  EXPECT_EQ(CaptureResolutionChooser::kDefaultCaptureSize,
            chooser.capture_size());

  chooser.SetSourceSize(gfx::Size());
  EXPECT_EQ(CaptureResolutionChooser::kDefaultCaptureSize,
            chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth, kMaxFrameHeight));
  EXPECT_EQ(CaptureResolutionChooser::kDefaultCaptureSize,
            chooser.capture_size());

  ExpectOnlyOneSnappedFrameSize(chooser,
                                CaptureResolutionChooser::kDefaultCaptureSize);
}

// While clients should always strive to set the source size before querying the
// CaptureResolutionChooser, do test that the reasonable "default" behavior is
// exhibited beforehand.
TEST(CaptureResolutionChooserTest, ReasonableCaptureSizeWhenMissingSourceSize) {
  CaptureResolutionChooser chooser;
  // CaptureResolutionChooser starts out with capture size set to
  // kDefaultCaptureSize.
  EXPECT_EQ(CaptureResolutionChooser::kDefaultCaptureSize,
            chooser.capture_size());

  // Setting constraints around kDefaultCaptureSize means the capture size
  // should remain as kDefaultCaptureSize.
  chooser.SetConstraints(gfx::Size(kMinFrameWidth, kMinFrameHeight),
                         gfx::Size(kMaxFrameWidth, kMaxFrameHeight), false);
  EXPECT_EQ(CaptureResolutionChooser::kDefaultCaptureSize,
            chooser.capture_size());

  // Setting constraints to a fixed size less than kDefaultCaptureSize will
  // change the capture size, to meet the required constraints range.
  chooser.SetConstraints(gfx::Size(kMinFrameWidth, kMinFrameHeight),
                         gfx::Size(kMinFrameWidth, kMinFrameHeight), false);
  EXPECT_EQ(gfx::Size(kMinFrameWidth, kMinFrameHeight), chooser.capture_size());

  // Setting the constraints back to the wide range should not change the
  // capture size, since the new range includes the former capture size.
  chooser.SetConstraints(gfx::Size(kMinFrameWidth, kMinFrameHeight),
                         gfx::Size(kMaxFrameWidth, kMaxFrameHeight), false);
  EXPECT_EQ(gfx::Size(kMinFrameWidth, kMinFrameHeight), chooser.capture_size());

  // Finally, updating the source size to be exactly in the middle of the
  // constraints range should result in the capture size being updated to that
  // same size.
  static constexpr gfx::Size middle_size(std::midpoint(kMinFrameWidth, kMaxFrameWidth),
                              std::midpoint(kMinFrameHeight, kMaxFrameHeight));
  chooser.SetSourceSize(middle_size);
  EXPECT_EQ(middle_size, chooser.capture_size());
}

TEST(CaptureResolutionChooserTest,
     FixedResolutionPolicy_CaptureSizeAlwaysFixed) {
  const gfx::Size the_one_frame_size(kMaxFrameWidth, kMaxFrameHeight);
  CaptureResolutionChooser chooser;
  chooser.SetConstraints(the_one_frame_size, the_one_frame_size, false);
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());

  chooser.SetSourceSize(the_one_frame_size);
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth + 424, kMaxFrameHeight - 101));
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth - 202, kMaxFrameHeight + 56));
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMinFrameWidth, kMinFrameHeight));
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());

  ExpectOnlyOneSnappedFrameSize(chooser, the_one_frame_size);

  // Ensure that changing the target frame area does not change the computed
  // frame size.
  chooser.SetTargetFrameArea(0);
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());
  chooser.SetTargetFrameArea(the_one_frame_size.GetArea() / 2);
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());
}

TEST(CaptureResolutionChooserTest,
     FixedAspectRatioPolicy_CaptureSizeHasSameAspectRatio) {
  const gfx::Size min_size(kMinFrameWidth, kMinFrameHeight);
  const gfx::Size max_size(kMaxFrameWidth, kMaxFrameHeight);
  CaptureResolutionChooser chooser;
  chooser.SetConstraints(min_size, max_size, true);

  // Starting condition.
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  // Max size in --> max size out.
  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth, kMaxFrameHeight));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  // Various source sizes within bounds.
  chooser.SetSourceSize(gfx::Size(640, 480));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(480, 640));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(640, 640));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  // Bad source size results in no update.
  const gfx::Size unchanged_size = chooser.capture_size();
  chooser.SetSourceSize(gfx::Size(0, 0));
  EXPECT_EQ(unchanged_size, chooser.capture_size());

  // Downscaling size (preserving aspect ratio) when source size exceeds the
  // upper bounds.
  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth * 2, kMaxFrameHeight * 2));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth * 2, kMaxFrameHeight));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth, kMaxFrameHeight * 2));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  // Upscaling size (preserving aspect ratio) when source size is under the
  // lower bounds.
  chooser.SetSourceSize(gfx::Size(kMinFrameWidth / 2, kMinFrameHeight / 2));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMinFrameWidth / 2, kMaxFrameHeight));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMinFrameWidth, kMinFrameHeight / 2));
  ExpectIsWithinBoundsAndSameAspectRatio(FROM_HERE, min_size, max_size,
                                         chooser.capture_size());

  // For a chooser configured with the "fixed aspect ratio" policy, the smallest
  // possible computed size is the one with 180 lines of resolution and the same
  // aspect ratio.
  const gfx::Size smallest_size(180 * kMaxFrameWidth / kMaxFrameHeight, 180);

  TestSnappedFrameSizes(&chooser, smallest_size);
  TestTargetedFrameAreas(&chooser, smallest_size);
}

TEST(CaptureResolutionChooserTest,
     AnyWithinLimitPolicy_CaptureSizeIsAnythingWithinLimits) {
  const gfx::Size min_size(kSmallestNonEmptyWidth, kSmallestNonEmptyHeight);
  const gfx::Size max_size(kMaxFrameWidth, kMaxFrameHeight);
  CaptureResolutionChooser chooser;
  chooser.SetConstraints(min_size, max_size, false);

  // Starting condition.
  EXPECT_EQ(CaptureResolutionChooser::kDefaultCaptureSize,
            chooser.capture_size());

  // Max size in --> max size out.
  chooser.SetSourceSize(max_size);
  EXPECT_EQ(max_size, chooser.capture_size());

  // Various source sizes within bounds.
  chooser.SetSourceSize(gfx::Size(640, 480));
  EXPECT_EQ(gfx::Size(640, 480), chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(480, 640));
  EXPECT_EQ(gfx::Size(480, 640), chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(640, 640));
  EXPECT_EQ(gfx::Size(640, 640), chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(2, 2));
  EXPECT_EQ(gfx::Size(2, 2), chooser.capture_size());

  // Bad source size results in no update.
  const gfx::Size unchanged_size = chooser.capture_size();
  chooser.SetSourceSize(gfx::Size(0, 0));
  EXPECT_EQ(unchanged_size, chooser.capture_size());

  // Downscaling size (preserving aspect ratio) when source size exceeds the
  // upper bounds.
  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth * 2, kMaxFrameHeight * 2));
  EXPECT_EQ(max_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth * 2, kMaxFrameHeight));
  EXPECT_EQ(gfx::Size(kMaxFrameWidth, kMaxFrameHeight / 2),
            chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth, kMaxFrameHeight * 2));
  EXPECT_EQ(gfx::Size(kMaxFrameWidth / 2, kMaxFrameHeight),
            chooser.capture_size());

  // For a chooser configured with the "any within limit" policy, the smallest
  // possible computed size is smallest non-empty snapped size (which is 90
  // lines of resolution) with the same aspect ratio as the maximum size.
  const gfx::Size smallest_size(90 * kMaxFrameWidth / kMaxFrameHeight, 90);

  TestSnappedFrameSizes(&chooser, smallest_size);
  TestTargetedFrameAreas(&chooser, smallest_size);
}

}  // namespace media
