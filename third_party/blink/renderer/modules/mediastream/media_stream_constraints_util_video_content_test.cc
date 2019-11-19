// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_content.h"

#include <cmath>
#include <string>

#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/mock_constraint_factory.h"

namespace blink {

// To avoid symbol collisions in jumbo builds.
namespace media_stream_constraints_util_video_content_test {

namespace {

const double kDefaultScreenCastAspectRatio =
    static_cast<double>(kDefaultScreenCastWidth) / kDefaultScreenCastHeight;

void CheckNonResolutionDefaults(const VideoCaptureSettings& result) {
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(base::Optional<double>(), result.min_frame_rate());
  EXPECT_EQ(base::Optional<double>(), result.max_frame_rate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
  EXPECT_FALSE(result.min_frame_rate().has_value());
}

void CheckNonFrameRateDefaults(const VideoCaptureSettings& result) {
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
}

void CheckTrackAdapterSettingsEqualsFormat(const VideoCaptureSettings& result) {
  // For content capture, resolution and frame rate should always be the same
  // for source and track.
  EXPECT_TRUE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
  EXPECT_EQ(result.Height(), result.track_adapter_settings().target_height());
  EXPECT_EQ(0.0, result.track_adapter_settings().max_frame_rate());
}

void CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(
    const VideoCaptureSettings& result) {
  EXPECT_EQ(
      static_cast<double>(kMinScreenCastDimension) / kMaxScreenCastDimension,
      result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(
      static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
      result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

}  // namespace

class MediaStreamConstraintsUtilVideoContentTest : public testing::Test {
 protected:
  VideoCaptureSettings SelectSettings(
      mojom::MediaStreamType stream_type =
          mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) {
    WebMediaConstraints constraints =
        constraint_factory_.CreateWebMediaConstraints();
    return SelectSettingsVideoContentCapture(constraints, stream_type,
                                             kDefaultScreenCastWidth,
                                             kDefaultScreenCastHeight);
  }

  MockConstraintFactory constraint_factory_;
};

// The Unconstrained test checks the default selection criteria.
TEST_F(MediaStreamConstraintsUtilVideoContentTest, Unconstrained) {
  constraint_factory_.Reset();
  auto result = SelectSettings();

  // All settings should have default values.
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

// The "Overconstrained" tests verify that failure of any single required
// constraint results in failure to select a candidate.
TEST_F(MediaStreamConstraintsUtilVideoContentTest, OverconstrainedOnHeight) {
  constraint_factory_.Reset();
  constraint_factory_.basic().height.SetExact(kMaxScreenCastDimension + 1);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().height.SetMin(kMaxScreenCastDimension + 1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().height.SetMax(kMinScreenCastDimension - 1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, OverconstrainedOnWidth) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetExact(kMaxScreenCastDimension + 1);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetMin(kMaxScreenCastDimension + 1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetMax(kMinScreenCastDimension - 1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       OverconstrainedOnAspectRatio) {
  constraint_factory_.Reset();
  constraint_factory_.basic().aspect_ratio.SetExact(123467890);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspect_ratio.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().aspect_ratio.SetMin(123467890);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspect_ratio.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().aspect_ratio.SetMax(0.00001);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspect_ratio.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, OverconstrainedOnFrameRate) {
  constraint_factory_.Reset();
  constraint_factory_.basic().frame_rate.SetExact(kMaxScreenCastFrameRate +
                                                  0.1);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frame_rate.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().frame_rate.SetMin(kMaxScreenCastFrameRate + 0.1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frame_rate.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().frame_rate.SetMax(-0.1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frame_rate.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       OverconstrainedOnInvalidResizeMode) {
  constraint_factory_.Reset();
  constraint_factory_.basic().resize_mode.SetExact(
      WebString::FromASCII("invalid"));
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().resize_mode.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       OverconstrainedOnEmptyResizeMode) {
  constraint_factory_.Reset();
  constraint_factory_.basic().resize_mode.SetExact(WebString::FromASCII(""));
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().resize_mode.GetName(),
            result.failed_constraint_name());
}

// The "Mandatory" and "Ideal" tests check that various selection criteria work
// for each individual constraint in the basic constraint set.
TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryDeviceID) {
  const std::string kDeviceID = "Some ID";
  constraint_factory_.Reset();
  constraint_factory_.basic().device_id.SetExact(
      WebString::FromASCII(kDeviceID));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kDeviceID, result.device_id());
  // Other settings should have default values.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealDeviceID) {
  const std::string kDeviceID = "Some ID";
  const std::string kIdealID = "Ideal ID";
  WebVector<WebString> device_ids(static_cast<size_t>(2));
  device_ids[0] = WebString::FromASCII(kDeviceID);
  device_ids[1] = WebString::FromASCII(kIdealID);
  constraint_factory_.Reset();
  constraint_factory_.basic().device_id.SetExact(device_ids);

  WebVector<WebString> ideal_id(static_cast<size_t>(1));
  ideal_id[0] = WebString::FromASCII(kIdealID);
  constraint_factory_.basic().device_id.SetIdeal(ideal_id);

  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kIdealID, result.device_id());
  // Other settings should have default values.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryNoiseReduction) {
  constraint_factory_.Reset();
  const bool kNoiseReductionValues[] = {true, false};
  for (auto noise_reduction : kNoiseReductionValues) {
    constraint_factory_.basic().goog_noise_reduction.SetExact(noise_reduction);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(noise_reduction, result.noise_reduction());
    // Other settings should have default values.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    EXPECT_EQ(std::string(), result.device_id());
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealNoiseReduction) {
  constraint_factory_.Reset();
  const bool kNoiseReductionValues[] = {true, false};
  for (auto noise_reduction : kNoiseReductionValues) {
    constraint_factory_.basic().goog_noise_reduction.SetIdeal(noise_reduction);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(noise_reduction, result.noise_reduction());
    // Other settings should have default values.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    EXPECT_EQ(std::string(), result.device_id());
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryExactHeight) {
  constraint_factory_.Reset();
  const int kHeight = 1000;
  constraint_factory_.basic().height.SetExact(kHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kHeight, result.Height());
  // The algorithm tries to preserve the default aspect ratio.
  EXPECT_EQ(std::round(kHeight * kDefaultScreenCastAspectRatio),
            result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(1.0 / kHeight, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kHeight,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMinHeight) {
  constraint_factory_.Reset();
  const int kHeight = 2000;
  constraint_factory_.basic().height.SetMin(kHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kHeight is greater that the default, so expect kHeight.
  EXPECT_EQ(kHeight, result.Height());
  EXPECT_EQ(std::round(kHeight * kDefaultScreenCastAspectRatio),
            result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(1.0 / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kHeight,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);

  const int kSmallHeight = 100;
  constraint_factory_.basic().height.SetMin(kSmallHeight);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kSmallHeight is less that the default, so expect the default.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(1.0 / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kSmallHeight,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMaxHeight) {
  // kMaxHeight smaller than the default.
  {
    constraint_factory_.Reset();
    const int kMaxHeight = kDefaultScreenCastHeight - 100;
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxHeight greater than the default.
  {
    constraint_factory_.Reset();
    const int kMaxHeight = kDefaultScreenCastHeight + 100;
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxHeight greater than the maximum allowed.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.SetMax(kMaxScreenCastDimension + 1);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(
        std::round(kDefaultScreenCastHeight * kDefaultScreenCastAspectRatio),
        result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxHeight equal to the maximum allowed.
  {
    constraint_factory_.Reset();
    const int kMaxHeight = kMaxScreenCastDimension;
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    // Since the given max is too large, the default aspect ratio cannot be
    // used and the width is clamped to the maximum.
    EXPECT_EQ(kMaxScreenCastDimension, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryHeightRange) {
  // Range includes the default.
  {
    constraint_factory_.Reset();
    const int kMinHeight = kDefaultScreenCastHeight - 100;
    const int kMaxHeight = kDefaultScreenCastHeight + 100;
    constraint_factory_.basic().height.SetMin(kMinHeight);
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // The whole range is greater than the default.
  {
    constraint_factory_.Reset();
    const int kMinHeight = kDefaultScreenCastHeight + 100;
    const int kMaxHeight = kDefaultScreenCastHeight + 200;
    constraint_factory_.basic().height.SetMin(kMinHeight);
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // The whole range is less than the default.
  {
    constraint_factory_.Reset();
    const int kMinHeight = kDefaultScreenCastHeight - 200;
    const int kMaxHeight = kDefaultScreenCastHeight - 100;
    constraint_factory_.basic().height.SetMin(kMinHeight);
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealHeight) {
  // Unconstrained.
  {
    constraint_factory_.Reset();
    const int kIdealHeight = 1000;
    constraint_factory_.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kIdealHeight, result.Height());
    // When ideal height is given, the algorithm returns a width that is closest
    // to height * kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kIdealHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal greater than maximum.
  {
    constraint_factory_.Reset();
    const int kIdealHeight = 1000;
    const int kMaxHeight = 800;
    constraint_factory_.basic().height.SetIdeal(kIdealHeight);
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal height is greater than the maximum, expect maximum.
    EXPECT_EQ(kMaxHeight, result.Height());
    // Expect closest to kMaxHeight * kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kMaxHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal less than minimum.
  {
    constraint_factory_.Reset();
    const int kIdealHeight = 1000;
    const int kMinHeight = 1200;
    constraint_factory_.basic().height.SetIdeal(kIdealHeight);
    constraint_factory_.basic().height.SetMin(kMinHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal height is less than the minimum, expect minimum.
    EXPECT_EQ(kMinHeight, result.Height());
    // Expect closest to kMinHeight * kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kMinHeight * kDefaultScreenCastAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal intersects a box.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.SetMin(500);
    constraint_factory_.basic().height.SetMax(1000);
    constraint_factory_.basic().width.SetMin(100);
    constraint_factory_.basic().width.SetMax(500);
    const int kIdealHeight = 750;
    constraint_factory_.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal height is included in the bounding box.
    EXPECT_EQ(kIdealHeight, result.Height());
    double default_aspect_ratio =
        static_cast<double>(constraint_factory_.basic().width.Max()) /
        constraint_factory_.basic().height.Max();
    // Expect width closest to kIdealHeight * default aspect ratio.
    EXPECT_EQ(std::round(kIdealHeight * default_aspect_ratio), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(100.0 / 1000.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(500.0 / 500.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the box, closest to the side coinciding with max height.
  {
    const int kMaxHeight = 1000;
    constraint_factory_.Reset();
    constraint_factory_.basic().height.SetMin(500);
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    constraint_factory_.basic().width.SetMin(100);
    constraint_factory_.basic().width.SetMax(500);
    constraint_factory_.basic().height.SetIdeal(1200);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxHeight, result.Height());
    // Expect width closest to kMaxHeight * default aspect ratio, which is
    // outside the box. Closest it max width.
    EXPECT_EQ(constraint_factory_.basic().width.Max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(100.0 / kMaxHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(500.0 / 500.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the constrained set, closest to a single point.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.SetMin(500);
    constraint_factory_.basic().height.SetMax(1000);
    constraint_factory_.basic().width.SetMin(500);
    constraint_factory_.basic().width.SetMax(1000);
    constraint_factory_.basic().aspect_ratio.SetMin(1.0);
    constraint_factory_.basic().height.SetIdeal(1200);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // (max-height, max-width) is the single point closest to the ideal line.
    EXPECT_EQ(constraint_factory_.basic().height.Max(), result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.Max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0, result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(1000.0 / 500.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryExactWidth) {
  constraint_factory_.Reset();
  const int kWidth = 1000;
  constraint_factory_.basic().width.SetExact(kWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kWidth, result.Width());
  EXPECT_EQ(std::round(kWidth / kDefaultScreenCastAspectRatio),
            result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kWidth) / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kWidth) / kMinScreenCastDimension,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMinWidth) {
  constraint_factory_.Reset();
  const int kWidth = 3000;
  constraint_factory_.basic().width.SetMin(kWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kWidth is greater that the default, so expect kWidth.
  EXPECT_EQ(kWidth, result.Width());
  EXPECT_EQ(std::round(kWidth / kDefaultScreenCastAspectRatio),
            result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kWidth) / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(
      static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
      result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);

  const int kSmallWidth = 100;
  constraint_factory_.basic().width.SetMin(kSmallWidth);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kSmallWidth is less that the default, so expect the default.
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kSmallWidth) / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(
      static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
      result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMaxWidth) {
  // kMaxWidth less than the default.
  {
    constraint_factory_.Reset();
    const int kMaxWidth = kDefaultScreenCastWidth - 100;
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // If max is provided, max is used as default.
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxWidth greater than the default.
  {
    constraint_factory_.Reset();
    const int kMaxWidth = kDefaultScreenCastWidth + 100;
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // If max is provided, max is used as default.
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxWidth greater than the maximum allowed (gets ignored).
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetMax(kMaxScreenCastDimension + 1);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Expect the default, since the given max value cannot be used as default.
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    EXPECT_EQ(
        std::round(kDefaultScreenCastWidth / kDefaultScreenCastAspectRatio),
        result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(
        static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
        result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // kMaxWidth equal to the maximum allowed.
  {
    constraint_factory_.Reset();
    const int kMaxWidth = kMaxScreenCastDimension;
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryWidthRange) {
  // The whole range is less than the default.
  {
    constraint_factory_.Reset();
    const int kMinWidth = kDefaultScreenCastWidth - 200;
    const int kMaxWidth = kDefaultScreenCastWidth - 100;
    constraint_factory_.basic().width.SetMin(kMinWidth);
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // The range includes the default.
  {
    constraint_factory_.Reset();
    const int kMinWidth = kDefaultScreenCastWidth - 100;
    const int kMaxWidth = kDefaultScreenCastWidth + 100;
    constraint_factory_.basic().width.SetMin(kMinWidth);
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // The whole range is greater than the default.
  {
    constraint_factory_.Reset();
    const int kMinWidth = kDefaultScreenCastWidth + 100;
    const int kMaxWidth = kDefaultScreenCastWidth + 200;
    constraint_factory_.basic().width.SetMin(kMinWidth);
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealWidth) {
  // Unconstrained
  {
    constraint_factory_.Reset();
    const int kIdealWidth = 1000;
    constraint_factory_.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kIdealWidth, result.Width());
    // When ideal width is given, the algorithm returns a height that is closest
    // to width / kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kIdealWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal greater than maximum.
  {
    constraint_factory_.Reset();
    const int kIdealWidth = 1000;
    const int kMaxWidth = 800;
    constraint_factory_.basic().width.SetIdeal(kIdealWidth);
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    // Expect closest to kMaxWidth / kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal less than minimum.
  {
    constraint_factory_.Reset();
    const int kIdealWidth = 1000;
    const int kMinWidth = 1200;
    constraint_factory_.basic().width.SetIdeal(kIdealWidth);
    constraint_factory_.basic().width.SetMin(kMinWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMinWidth, result.Width());
    // Expect closest to kMinWidth / kDefaultScreenCastAspectRatio.
    EXPECT_EQ(std::round(kMinWidth / kDefaultScreenCastAspectRatio),
              result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(
        static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
        result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal intersects a box.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetMin(500);
    constraint_factory_.basic().width.SetMax(1000);
    constraint_factory_.basic().height.SetMin(100);
    constraint_factory_.basic().height.SetMax(500);
    const int kIdealWidth = 750;
    constraint_factory_.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal width is included in the bounding box.
    EXPECT_EQ(kIdealWidth, result.Width());
    // Expect height closest to kIdealWidth / default aspect ratio.
    double default_aspect_ratio =
        static_cast<double>(constraint_factory_.basic().width.Max()) /
        constraint_factory_.basic().height.Max();
    EXPECT_EQ(std::round(kIdealWidth / default_aspect_ratio), result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(500.0 / 500.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(1000.0 / 100.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the box, closest to the side coinciding with max width.
  {
    const int kMaxWidth = 1000;
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetMin(500);
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    constraint_factory_.basic().height.SetMin(100);
    constraint_factory_.basic().height.SetMax(500);
    constraint_factory_.basic().width.SetIdeal(1200);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxWidth, result.Width());
    // kMaxWidth / kDefaultScreenCastAspectRatio is outside the box. Closest is
    // max height.
    EXPECT_EQ(constraint_factory_.basic().height.Max(), result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(500.0 / 500.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMaxWidth) / 100.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the constrained set, closest to a single point.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetMin(100);
    constraint_factory_.basic().width.SetMax(500);
    constraint_factory_.basic().height.SetMin(100);
    constraint_factory_.basic().height.SetMax(500);
    constraint_factory_.basic().aspect_ratio.SetMax(1.0);
    constraint_factory_.basic().width.SetIdeal(1200);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // (max-width, max-height) is the single point closest to the ideal line.
    EXPECT_EQ(constraint_factory_.basic().width.Max(), result.Width());
    EXPECT_EQ(constraint_factory_.basic().height.Max(), result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(100.0 / 500.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(1.0, result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryExactAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 2.0;
  constraint_factory_.basic().aspect_ratio.SetExact(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Given that the default aspect ratio cannot be preserved, the algorithm
  // tries to preserve, among the default height or width, the one that leads
  // to highest area. In this case, height is preserved.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(std::round(kDefaultScreenCastHeight * kAspectRatio),
            result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMinAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 2.0;
  constraint_factory_.basic().aspect_ratio.SetMin(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kAspectRatio is greater that the default, so expect kAspectRatio.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(std::round(kDefaultScreenCastHeight * kAspectRatio),
            result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) /
                static_cast<double>(kMinScreenCastDimension),
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);

  const double kSmallAspectRatio = 0.5;
  constraint_factory_.basic().aspect_ratio.SetMin(kSmallAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kSmallAspectRatio is less that the default, so expect the default.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(kSmallAspectRatio,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) /
                static_cast<double>(kMinScreenCastDimension),
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMaxAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 2.0;
  constraint_factory_.basic().aspect_ratio.SetMax(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kAspectRatio is greater that the default, so expect the default.
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kMinScreenCastDimension) /
                static_cast<double>(kMaxScreenCastDimension),
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);

  const double kSmallAspectRatio = 0.5;
  constraint_factory_.basic().aspect_ratio.SetMax(kSmallAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kSmallAspectRatio is less that the default, so expect kSmallAspectRatio.
  // Prefer to preserve default width since that leads to larger area than
  // preserving default height.
  EXPECT_EQ(std::round(kDefaultScreenCastWidth / kSmallAspectRatio),
            result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(static_cast<double>(kMinScreenCastDimension) /
                static_cast<double>(kMaxScreenCastDimension),
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(kSmallAspectRatio,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryRangeAspectRatio) {
  constraint_factory_.Reset();
  {
    const double kMinAspectRatio = 0.5;
    const double kMaxAspectRatio = 2.0;
    constraint_factory_.basic().aspect_ratio.SetMin(kMinAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Range includes default, so expect the default.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  {
    const double kMinAspectRatio = 2.0;
    const double kMaxAspectRatio = 3.0;
    constraint_factory_.basic().aspect_ratio.SetMin(kMinAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The whole range is greater than the default. Expect the minimum.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(std::round(kDefaultScreenCastHeight * kMinAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  {
    const double kMinAspectRatio = 0.5;
    const double kMaxAspectRatio = 1.0;
    constraint_factory_.basic().aspect_ratio.SetMin(kMinAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The whole range is less than the default. Expect the maximum.
    EXPECT_EQ(std::round(kDefaultScreenCastWidth / kMaxAspectRatio),
              result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealAspectRatio) {
  // Unconstrained.
  {
    constraint_factory_.Reset();
    const double kIdealAspectRatio = 2.0;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(std::round(kDefaultScreenCastHeight * kIdealAspectRatio),
              result.Width());
    CheckNonResolutionDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal greater than maximum.
  {
    constraint_factory_.Reset();
    const double kIdealAspectRatio = 2.0;
    const double kMaxAspectRatio = 1.5;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect ratio is greater than the maximum, expect maximum.
    EXPECT_EQ(std::round(kDefaultScreenCastWidth / kMaxAspectRatio),
              result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(
        static_cast<double>(kMinScreenCastDimension) / kMaxScreenCastDimension,
        result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal less than minimum.
  {
    constraint_factory_.Reset();
    const double kIdealAspectRatio = 1.0;
    const double kMinAspectRatio = 1.5;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetMin(kMinAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect ratio is less than the maximum, expect minimum.
    EXPECT_EQ(std::round(kDefaultScreenCastWidth / kMinAspectRatio),
              result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(
        static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
        result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal intersects a box.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.SetMin(100);
    constraint_factory_.basic().height.SetMax(500);
    constraint_factory_.basic().width.SetMin(100);
    constraint_factory_.basic().width.SetMax(500);
    const double kIdealAspectRatio = 2.0;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect-ratio is included in the bounding box, with the value
    // closest to a standard width or height being the cut with the maximum
    // width.
    EXPECT_EQ(
        std::round(constraint_factory_.basic().width.Max() / kIdealAspectRatio),
        result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.Max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(100.0 / 500.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(500.0 / 100.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.basic().height.SetMin(1000);
    constraint_factory_.basic().height.SetMax(5000);
    constraint_factory_.basic().width.SetMin(1000);
    constraint_factory_.basic().width.SetMax(5000);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect-ratio is included in the bounding box.
    EXPECT_EQ(
        std::round(constraint_factory_.basic().width.Max() / kIdealAspectRatio),
        result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.Max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1000.0 / 5000.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(5000.0 / 1000.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.Reset();
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    constraint_factory_.basic().height.SetMin(250);
    constraint_factory_.basic().width.SetMin(250);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal aspect-ratio is included in the bounding box. Preserving default
    // height leads to larger area than preserving default width.
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastHeight * kIdealAspectRatio, result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(250.0 / kMaxScreenCastDimension,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxScreenCastDimension / 250.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the constrained area, closest to min or max aspect ratio.
  {
    const double kMinAspectRatio = 0.5;
    const double kMaxAspectRatio = 2.0;
    constraint_factory_.Reset();
    constraint_factory_.basic().height.SetMin(100);
    constraint_factory_.basic().height.SetMax(500);
    constraint_factory_.basic().width.SetMin(100);
    constraint_factory_.basic().width.SetMax(500);
    constraint_factory_.basic().aspect_ratio.SetMin(kMinAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetMax(kMaxAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetIdeal(3.0);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to kMaxAspectRatio.
    EXPECT_EQ(
        std::round(constraint_factory_.basic().width.Max() / kMaxAspectRatio),
        result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.Max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.basic().aspect_ratio.SetIdeal(0.3);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to kMinAspectRatio.
    EXPECT_EQ(constraint_factory_.basic().height.Max(), result.Height());
    EXPECT_EQ(
        std::round(constraint_factory_.basic().height.Max() * kMinAspectRatio),
        result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);

    // Use a box that is bigger and further from the origin to force closeness
    // to a different default dimension.
    constraint_factory_.Reset();
    constraint_factory_.basic().aspect_ratio.SetMin(kMinAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetMax(kMaxAspectRatio);
    constraint_factory_.basic().height.SetMin(3000);
    constraint_factory_.basic().width.SetMin(3000);
    constraint_factory_.basic().aspect_ratio.SetIdeal(3.0);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to kMaxAspectRatio.
    EXPECT_EQ(constraint_factory_.basic().height.Min(), result.Height());
    EXPECT_EQ(
        std::round(constraint_factory_.basic().height.Min() * kMaxAspectRatio),
        result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.basic().aspect_ratio.SetIdeal(0.3);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to kMinAspectRatio.
    EXPECT_EQ(
        std::round(constraint_factory_.basic().width.Min() / kMinAspectRatio),
        result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.Min(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  // Ideal outside the constrained area, closest to a single point.
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.SetMin(100);
    constraint_factory_.basic().height.SetMax(500);
    constraint_factory_.basic().width.SetMin(100);
    constraint_factory_.basic().width.SetMax(500);
    constraint_factory_.basic().aspect_ratio.SetMin(1.0);
    constraint_factory_.basic().aspect_ratio.SetIdeal(10.0);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Ideal is closest to the min height and max width.
    EXPECT_EQ(constraint_factory_.basic().height.Min(), result.Height());
    EXPECT_EQ(constraint_factory_.basic().width.Max(), result.Width());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1.0, result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(500.0 / 100.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryExactFrameRate) {
  constraint_factory_.Reset();
  const double kFrameRate = 45.0;
  constraint_factory_.basic().frame_rate.SetExact(kFrameRate);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kFrameRate, result.FrameRate());
  EXPECT_EQ(kFrameRate, result.min_frame_rate());
  EXPECT_EQ(kFrameRate, result.max_frame_rate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMinFrameRate) {
  // MinFrameRate greater than the default frame rate.
  {
    constraint_factory_.Reset();
    const double kMinFrameRate = 45.0;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // kMinFrameRate is greater that the default, so expect kMinFrameRate.
    EXPECT_EQ(kMinFrameRate, result.FrameRate());
    EXPECT_TRUE(result.min_frame_rate().has_value());
    EXPECT_EQ(kMinFrameRate, result.min_frame_rate());
    EXPECT_FALSE(result.max_frame_rate().has_value());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // MinFrameRate less than the default frame rate.
  {
    const double kMinFrameRate = 5.0;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // No ideal or maximum frame rate given, expect default.
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    EXPECT_TRUE(result.min_frame_rate().has_value());
    EXPECT_EQ(kMinFrameRate, result.min_frame_rate());
    EXPECT_FALSE(result.max_frame_rate().has_value());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // MinFrameRate less than the minimum allowed.
  {
    const double kMinFrameRate = -0.01;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // No ideal or maximum frame rate given, expect default.
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    // kMinFrameRate should be ignored.
    EXPECT_FALSE(result.min_frame_rate().has_value());
    EXPECT_FALSE(result.max_frame_rate().has_value());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // MinFrameRate equal to the minimum allowed.
  {
    const double kMinFrameRate = 0.0;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // No ideal or maximum frame rate given, expect default.
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    EXPECT_TRUE(result.min_frame_rate().has_value());
    EXPECT_EQ(kMinFrameRate, result.min_frame_rate());
    EXPECT_FALSE(result.max_frame_rate().has_value());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryMaxFrameRate) {
  constraint_factory_.Reset();
  // kMaxFrameRate greater than default
  {
    const double kMaxFrameRate = 45.0;
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // If max frame rate is provided, it is used as default.
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    EXPECT_EQ(base::Optional<double>(), result.min_frame_rate());
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // kMaxFrameRate less than default
  {
    const double kMaxFrameRate = 5.0;
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // If max frame rate is provided, it is used as default.
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    EXPECT_EQ(base::Optional<double>(), result.min_frame_rate());
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // kMaxFrameRate greater than the maximum allowed
  {
    const double kMaxFrameRate = kMaxScreenCastFrameRate + 0.1;
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Expect the default, since the given maximum is invalid.
    EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
    EXPECT_EQ(base::Optional<double>(), result.min_frame_rate());
    EXPECT_EQ(base::Optional<double>(), result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // kMaxFrameRate equal to the maximum allowed
  {
    const double kMaxFrameRate = kMaxScreenCastFrameRate;
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // If max frame rate is provided, it is used as default.
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    EXPECT_EQ(base::Optional<double>(), result.min_frame_rate());
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryRangeFrameRate) {
  constraint_factory_.Reset();
  {
    const double kMinFrameRate = 15.0;
    const double kMaxFrameRate = 45.0;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // If max frame rate is provided, it is used as default.
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    EXPECT_EQ(kMinFrameRate, result.min_frame_rate());
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  {
    const double kMinFrameRate = 45.0;
    const double kMaxFrameRate = 55.0;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // If max frame rate is provided, it is used as default.
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    EXPECT_EQ(kMinFrameRate, result.min_frame_rate());
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  {
    const double kMinFrameRate = 10.0;
    const double kMaxFrameRate = 15.0;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // If max frame rate is provided, it is used as default.
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    EXPECT_EQ(kMinFrameRate, result.min_frame_rate());
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, IdealFrameRate) {
  // Unconstrained.
  {
    constraint_factory_.Reset();
    const double kIdealFrameRate = 45.0;
    constraint_factory_.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kIdealFrameRate, result.FrameRate());
    EXPECT_EQ(base::Optional<double>(), result.min_frame_rate());
    EXPECT_EQ(base::Optional<double>(), result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal greater than maximum.
  {
    constraint_factory_.Reset();
    const double kIdealFrameRate = 45.0;
    const double kMaxFrameRate = 30.0;
    constraint_factory_.basic().frame_rate.SetIdeal(kIdealFrameRate);
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    EXPECT_EQ(base::Optional<double>(), result.min_frame_rate());
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal less than minimum.
  {
    constraint_factory_.Reset();
    const double kIdealFrameRate = 45.0;
    const double kMinFrameRate = 50.0;
    constraint_factory_.basic().frame_rate.SetIdeal(kIdealFrameRate);
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kMinFrameRate, result.FrameRate());
    EXPECT_EQ(base::Optional<double>(), result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }

  // Ideal within range.
  {
    constraint_factory_.Reset();
    const double kIdealFrameRate = 45.0;
    const double kMinFrameRate = 35.0;
    const double kMaxFrameRate = 50.0;
    constraint_factory_.basic().frame_rate.SetIdeal(kIdealFrameRate);
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kIdealFrameRate, result.FrameRate());
    EXPECT_EQ(kMinFrameRate, result.min_frame_rate());
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate());
    CheckNonFrameRateDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, MandatoryResizeMode) {
  const int kIdealWidth = 641;
  const int kIdealHeight = 480;
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetIdeal(kIdealWidth);
  constraint_factory_.basic().height.SetIdeal(kIdealHeight);
  constraint_factory_.basic().resize_mode.SetExact(
      WebString::FromASCII("none"));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Screen capture will proceed at 641x480, which will be considered "native".
  // No rescaling will occur since it is explicitly disabled.
  EXPECT_EQ(result.Width(), kIdealWidth);
  EXPECT_EQ(result.Height(), kIdealHeight);
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());

  constraint_factory_.basic().resize_mode.SetExact(
      WebString::FromASCII("crop-and-scale"));
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(result.Width(), kIdealWidth);
  EXPECT_EQ(result.Height(), kIdealHeight);
  EXPECT_EQ(result.track_adapter_settings().target_width(), kIdealWidth);
  EXPECT_EQ(result.track_adapter_settings().target_height(), kIdealHeight);
}

// The "Advanced" tests check selection criteria involving advanced constraint
// sets.
TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedMinMaxResolutionFrameRate) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.width.SetMin(2000000000);
  advanced1.height.SetMin(2000000000);
  // The first advanced set cannot be satisfied and is therefore ignored in all
  // calls to SelectSettings().
  // In this case, default settings must be selected.
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
  CheckNonResolutionDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);

  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.height.SetMax(400);
  advanced2.width.SetMax(500);
  advanced2.aspect_ratio.SetExact(5.0 / 4.0);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(400, result.Height());
  EXPECT_EQ(500, result.Width());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);

  WebMediaTrackConstraintSet& advanced3 = constraint_factory_.AddAdvanced();
  advanced3.frame_rate.SetMax(10.0);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The third advanced set is supported in addition to the previous set.
  EXPECT_EQ(400, result.Height());
  EXPECT_EQ(500, result.Width());
  EXPECT_EQ(10.0, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);

  WebMediaTrackConstraintSet& advanced4 = constraint_factory_.AddAdvanced();
  advanced4.width.SetExact(1000);
  advanced4.height.SetExact(1000);
  result = SelectSettings();
  // The fourth advanced set cannot be supported in combination with the
  // previous two sets, so it must be ignored.
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(400, result.Height());
  EXPECT_EQ(500, result.Width());
  EXPECT_EQ(10.0, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);

  constraint_factory_.basic().width.SetIdeal(100);
  constraint_factory_.basic().height.SetIdeal(100);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The closest point to (100, 100) that satisfies all previous constraint
  // sets is its projection on the aspect-ratio line 5.0/4.0.
  // This is a point m*(4, 5) such that Dot((4,5), (100 - m(4,5))) == 0.
  // This works out to be m = 900/41.
  EXPECT_EQ(std::round(4.0 * 900.0 / 41.0), result.Height());
  EXPECT_EQ(std::round(5.0 * 900.0 / 41.0), result.Width());
  EXPECT_EQ(10.0, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);

  constraint_factory_.basic().width.SetIdeal(2000);
  constraint_factory_.basic().height.SetIdeal(1500);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The projection of (2000,1500) on the aspect-ratio line 5.0/4.0 is beyond
  // the maximum of (400, 500), so use the maximum allowed resolution.
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(400, result.Height());
  EXPECT_EQ(500, result.Width());
  EXPECT_EQ(10.0, result.FrameRate());
  EXPECT_EQ(base::Optional<bool>(), result.noise_reduction());
  EXPECT_EQ(std::string(), result.device_id());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(5.0 / 4.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, AdvancedExactResolution) {
  {
    constraint_factory_.Reset();
    WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
    advanced1.width.SetExact(40000000);
    advanced1.height.SetExact(40000000);
    WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
    advanced2.width.SetExact(300000000);
    advanced2.height.SetExact(300000000);
    auto result = SelectSettings();
    // None of the constraint sets can be satisfied. Default resolution should
    // be selected.
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    CheckNonResolutionDefaults(result);
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);

    WebMediaTrackConstraintSet& advanced3 = constraint_factory_.AddAdvanced();
    advanced3.width.SetExact(1920);
    advanced3.height.SetExact(1080);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);

    WebMediaTrackConstraintSet& advanced4 = constraint_factory_.AddAdvanced();
    advanced4.width.SetExact(640);
    advanced4.height.SetExact(480);
    result = SelectSettings();
    // The fourth constraint set contradicts the third set. The fourth set
    // should be ignored.
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);

    constraint_factory_.basic().width.SetIdeal(800);
    constraint_factory_.basic().height.SetIdeal(600);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The exact constraints has priority over ideal.
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    CheckNonResolutionDefaults(result);
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(1920.0 / 1080.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedResolutionAndFrameRate) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.width.SetExact(1920);
  advanced1.height.SetExact(1080);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.frame_rate.SetExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(1920, result.Width());
  EXPECT_EQ(1080, result.Height());
  EXPECT_EQ(60.0, result.FrameRate());
  EXPECT_EQ(1920.0 / 1080.0,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(1920.0 / 1080.0,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, AdvancedNoiseReduction) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.width.SetMin(640);
  advanced1.height.SetMin(480);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  const int kMinWidth = 4000;
  const int kMinHeight = 2000;
  advanced2.width.SetMin(kMinWidth);
  advanced2.height.SetMin(kMinHeight);
  advanced2.goog_noise_reduction.SetExact(false);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kMinWidth, result.Width());
  // Preserves default aspect ratio.
  EXPECT_EQ(static_cast<int>(
                std::round(result.Width() / kDefaultScreenCastAspectRatio)),
            result.Height());
  EXPECT_TRUE(result.noise_reduction() && !*result.noise_reduction());
  EXPECT_EQ(kMinWidth / static_cast<double>(kMaxScreenCastDimension),
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

// The "AdvancedContradictory" tests check that advanced constraint sets that
// contradict previous constraint sets are ignored.
TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryNoiseReduction) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.width.SetExact(640);
  advanced1.height.SetExact(480);
  advanced1.goog_noise_reduction.SetExact(true);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.width.SetExact(1920);
  advanced2.height.SetExact(1080);
  advanced2.goog_noise_reduction.SetExact(false);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  EXPECT_TRUE(result.noise_reduction() && *result.noise_reduction());
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryExactResolution) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.width.SetExact(640);
  advanced1.height.SetExact(480);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.width.SetExact(1920);
  advanced2.height.SetExact(1080);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryMaxMinResolutionFrameRate) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.width.SetMax(640);
  advanced1.height.SetMax(480);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.width.SetMin(1920);
  advanced2.height.SetMin(1080);
  advanced2.frame_rate.SetExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  // Resolution cannot exceed the requested resolution.
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(kMinScreenCastDimension / 480.0,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(640.0 / kMinScreenCastDimension,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryMinMaxResolutionFrameRate) {
  const int kMinHeight = 2600;
  const int kMinWidth = 2800;
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.width.SetMin(kMinWidth);
  advanced1.height.SetMin(kMinHeight);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.width.SetMax(640);
  advanced2.height.SetMax(480);
  advanced2.frame_rate.SetExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(std::round(kMinHeight * kDefaultScreenCastAspectRatio),
            result.Width());
  EXPECT_EQ(kMinHeight, result.Height());
  EXPECT_EQ(kDefaultScreenCastFrameRate, result.FrameRate());
  EXPECT_EQ(static_cast<double>(kMinWidth) / kMaxScreenCastDimension,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kMaxScreenCastDimension) / kMinHeight,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryExactAspectRatio) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  const double kMinAspectRatio = 5.0;
  advanced1.aspect_ratio.SetExact(kMinAspectRatio);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.aspect_ratio.SetExact(3.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(std::round(kDefaultScreenCastHeight * kMinAspectRatio),
            result.Width());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(kMinAspectRatio,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(kMinAspectRatio,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryAspectRatioRange) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  const double kMinAspectRatio = 5.0;
  advanced1.aspect_ratio.SetMin(kMinAspectRatio);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.aspect_ratio.SetMax(3.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(std::round(kDefaultScreenCastHeight * kMinAspectRatio),
            result.Width());
  EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
  CheckNonResolutionDefaults(result);
  EXPECT_EQ(kMinAspectRatio,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(
      kMaxScreenCastDimension / static_cast<double>(kMinScreenCastDimension),
      result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryExactFrameRate) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.frame_rate.SetExact(40.0);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.frame_rate.SetExact(45.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(40.0, result.FrameRate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryFrameRateRange) {
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.frame_rate.SetMin(40.0);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.frame_rate.SetMax(35.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_LE(40.0, result.FrameRate());
  CheckNonFrameRateDefaults(result);
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryWidthFrameRate) {
  const int kMaxWidth = 1920;
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.width.SetMax(kMaxWidth);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.width.SetMin(2000);
  advanced2.frame_rate.SetExact(10.0);
  WebMediaTrackConstraintSet& advanced3 = constraint_factory_.AddAdvanced();
  advanced3.frame_rate.SetExact(90.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kMaxWidth, result.Width());
  EXPECT_EQ(std::round(kMaxWidth / kDefaultScreenCastAspectRatio),
            result.Height());
  EXPECT_EQ(90.0, result.FrameRate());
  EXPECT_EQ(
      static_cast<double>(kMinScreenCastDimension) / kMaxScreenCastDimension,
      result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kMaxWidth) / kMinScreenCastDimension,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryHeightFrameRate) {
  const int kMaxHeight = 2000;
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  advanced1.height.SetMax(kMaxHeight);
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.height.SetMin(4500);
  advanced2.frame_rate.SetExact(10.0);
  WebMediaTrackConstraintSet& advanced3 = constraint_factory_.AddAdvanced();
  advanced3.frame_rate.SetExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(kMaxHeight * kDefaultScreenCastAspectRatio, result.Width());
  // Height defaults to explicitly given max constraint.
  EXPECT_EQ(kMaxHeight, result.Height());
  EXPECT_EQ(60.0, result.FrameRate());
  EXPECT_EQ(static_cast<double>(kMinScreenCastDimension) / kMaxHeight,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(
      static_cast<double>(kMaxScreenCastDimension) / kMinScreenCastDimension,
      result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, AdvancedDeviceID) {
  const std::string kDeviceID1 = "fake_device_1";
  const std::string kDeviceID2 = "fake_device_2";
  const std::string kDeviceID3 = "fake_device_3";
  const std::string kDeviceID4 = "fake_device_4";
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  WebString id_vector1[] = {WebString::FromASCII(kDeviceID1),
                            WebString::FromASCII(kDeviceID2)};
  advanced1.device_id.SetExact(
      WebVector<WebString>(id_vector1, base::size(id_vector1)));
  WebString id_vector2[] = {WebString::FromASCII(kDeviceID2),
                            WebString::FromASCII(kDeviceID3)};
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.device_id.SetExact(
      WebVector<WebString>(id_vector2, base::size(id_vector2)));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kDeviceID2 must be selected because it is the only one that satisfies both
  // advanced sets.
  EXPECT_EQ(kDeviceID2, result.device_id());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest,
       AdvancedContradictoryDeviceID) {
  const std::string kDeviceID1 = "fake_device_1";
  const std::string kDeviceID2 = "fake_device_2";
  const std::string kDeviceID3 = "fake_device_3";
  const std::string kDeviceID4 = "fake_device_4";
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced1 = constraint_factory_.AddAdvanced();
  WebString id_vector1[] = {WebString::FromASCII(kDeviceID1),
                            WebString::FromASCII(kDeviceID2)};
  advanced1.device_id.SetExact(
      WebVector<WebString>(id_vector1, base::size(id_vector1)));
  WebString id_vector2[] = {WebString::FromASCII(kDeviceID3),
                            WebString::FromASCII(kDeviceID4)};
  WebMediaTrackConstraintSet& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.device_id.SetExact(
      WebVector<WebString>(id_vector2, base::size(id_vector2)));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set.
  EXPECT_EQ(std::string(kDeviceID1), result.device_id());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, AdvancedIdealDeviceID) {
  const std::string kDeviceID1 = "fake_device_1";
  const std::string kDeviceID2 = "fake_device_2";
  const std::string kDeviceID3 = "fake_device_3";
  constraint_factory_.Reset();
  WebMediaTrackConstraintSet& advanced = constraint_factory_.AddAdvanced();
  WebString id_vector1[] = {WebString::FromASCII(kDeviceID1),
                            WebString::FromASCII(kDeviceID2)};
  advanced.device_id.SetExact(
      WebVector<WebString>(id_vector1, base::size(id_vector1)));

  WebString id_vector2[] = {WebString::FromASCII(kDeviceID2),
                            WebString::FromASCII(kDeviceID3)};
  constraint_factory_.basic().device_id.SetIdeal(
      WebVector<WebString>(id_vector2, base::size(id_vector2)));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Should select kDeviceID2, which appears in ideal and satisfies the advanced
  // set.
  EXPECT_EQ(std::string(kDeviceID2), result.device_id());
  CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, AdvancedResizeMode) {
  const int kIdealWidth = 641;
  const int kIdealHeight = 480;
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetIdeal(kIdealWidth);
  constraint_factory_.basic().height.SetIdeal(kIdealHeight);
  WebMediaTrackConstraintSet& advanced = constraint_factory_.AddAdvanced();
  advanced.resize_mode.SetExact(WebString::FromASCII("none"));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Screen capture will proceed at 641x480, which will be considered "native".
  // No rescaling will occur since it is explicitly disabled in the advanced
  // constraint set.
  EXPECT_EQ(result.Width(), kIdealWidth);
  EXPECT_EQ(result.Height(), kIdealHeight);
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());

  advanced.resize_mode.SetExact(WebString::FromASCII("crop-and-scale"));
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Screen capture will proceed at 641x480, which will be considered "native".
  // No rescaling will occur since it is explicitly disabled in the advanced
  // constraint set.
  EXPECT_EQ(result.Width(), kIdealWidth);
  EXPECT_EQ(result.Height(), kIdealHeight);
  EXPECT_TRUE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(result.track_adapter_settings().target_width(), kIdealWidth);
  EXPECT_EQ(result.track_adapter_settings().target_height(), kIdealHeight);
}

TEST_F(MediaStreamConstraintsUtilVideoContentTest, ResolutionChangePolicy) {
  {
    constraint_factory_.Reset();
    auto result = SelectSettings();
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    // Resolution can be adjusted.
    EXPECT_EQ(media::ResolutionChangePolicy::ANY_WITHIN_LIMIT,
              result.ResolutionChangePolicy());
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
  {
    constraint_factory_.Reset();
    auto result = SelectSettings(mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE);
    EXPECT_EQ(kDefaultScreenCastWidth, result.Width());
    EXPECT_EQ(kDefaultScreenCastHeight, result.Height());
    // Default policy for tab capture is fixed resolution.
    EXPECT_EQ(media::ResolutionChangePolicy::FIXED_RESOLUTION,
              result.ResolutionChangePolicy());
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetIdeal(630);
    constraint_factory_.basic().height.SetIdeal(470);
    auto result = SelectSettings();
    EXPECT_EQ(630, result.Width());
    EXPECT_EQ(470, result.Height());
    // Resolution can be adjusted because ideal was used to select the
    // resolution.
    EXPECT_EQ(media::ResolutionChangePolicy::ANY_WITHIN_LIMIT,
              result.ResolutionChangePolicy());
    CheckTrackAdapterSettingsEqualsFormatDefaultAspectRatio(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetExact(640);
    constraint_factory_.basic().height.SetExact(480);
    auto result = SelectSettings();
    EXPECT_EQ(640, result.Width());
    EXPECT_EQ(480, result.Height());
    EXPECT_EQ(media::ResolutionChangePolicy::FIXED_RESOLUTION,
              result.ResolutionChangePolicy());
    EXPECT_EQ(640.0 / 480.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(640.0 / 480.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetExact(1000);
    constraint_factory_.basic().height.SetExact(500);
    auto result = SelectSettings();
    EXPECT_EQ(1000, result.Width());
    EXPECT_EQ(500, result.Height());
    EXPECT_EQ(media::ResolutionChangePolicy::FIXED_RESOLUTION,
              result.ResolutionChangePolicy());
    EXPECT_EQ(1000.0 / 500.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(1000.0 / 500.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetExact(630);
    constraint_factory_.basic().height.SetExact(470);
    auto result = SelectSettings();
    EXPECT_EQ(630, result.Width());
    EXPECT_EQ(470, result.Height());
    EXPECT_EQ(media::ResolutionChangePolicy::FIXED_RESOLUTION,
              result.ResolutionChangePolicy());
    EXPECT_EQ(630.0 / 470.0,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(630.0 / 470.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetMax(800);
    constraint_factory_.basic().height.SetMax(600);
    constraint_factory_.basic().width.SetMin(400);
    constraint_factory_.basic().height.SetMin(300);
    auto result = SelectSettings();
    EXPECT_EQ(800, result.Width());
    EXPECT_EQ(600, result.Height());
    // When the aspect ratio of the max resolution equals the aspect ratio of
    // the min resolution, the algorithm sets fixed aspect ratio policy.
    EXPECT_EQ(media::ResolutionChangePolicy::FIXED_ASPECT_RATIO,
              result.ResolutionChangePolicy());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().width.SetMax(800);
    constraint_factory_.basic().height.SetMax(600);
    constraint_factory_.basic().width.SetMin(400);
    constraint_factory_.basic().height.SetMin(400);
    auto result = SelectSettings();
    EXPECT_EQ(800, result.Width());
    EXPECT_EQ(600, result.Height());
    // When the aspect ratio of the max resolution differs from the aspect ratio
    // of the min resolution, the algorithm sets any-within-limit policy.
    EXPECT_EQ(media::ResolutionChangePolicy::ANY_WITHIN_LIMIT,
              result.ResolutionChangePolicy());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
  {
    constraint_factory_.Reset();
    constraint_factory_.basic().height.SetMax(4000);
    constraint_factory_.basic().width.SetMax(4000);
    auto result = SelectSettings();
    EXPECT_EQ(4000, result.Width());
    EXPECT_EQ(4000, result.Height());
    // Only specifying a maximum resolution allows resolution adjustment.
    EXPECT_EQ(media::ResolutionChangePolicy::ANY_WITHIN_LIMIT,
              result.ResolutionChangePolicy());
    EXPECT_EQ(1.0 / 4000, result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(4000.0, result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

}  // namespace media_stream_constraints_util_video_content_test
}  // namespace blink
