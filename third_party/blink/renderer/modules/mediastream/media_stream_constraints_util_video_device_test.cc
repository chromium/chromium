// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/mock_constraint_factory.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

const char kDeviceID1[] = "fake_device_1";
const char kDeviceID2[] = "fake_device_2";
const char kDeviceID3[] = "fake_device_3";
const char kDeviceID4[] = "fake_device_4";
const char kDeviceID5[] = "fake_device_5";

const char kGroupID1[] = "fake_group_1";
const char kGroupID2[] = "fake_group_2";
const char kGroupID3[] = "fake_group_3";
const char kGroupID4[] = "fake_group_4";
const char kGroupID5[] = "fake_group_5";

void CheckTrackAdapterSettingsEqualsResolution(
    const VideoCaptureSettings& settings) {
  EXPECT_FALSE(settings.track_adapter_settings().target_size());
  EXPECT_EQ(1.0 / settings.Format().frame_size.height(),
            settings.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(settings.Format().frame_size.width(),
            settings.track_adapter_settings().max_aspect_ratio());
}

void CheckTrackAdapterSettingsEqualsFrameRate(
    const VideoCaptureSettings& settings,
    std::optional<double> value = std::nullopt) {
  EXPECT_EQ(value, settings.track_adapter_settings().max_frame_rate());
}

void CheckTrackAdapterSettingsEqualsFormat(
    const VideoCaptureSettings& settings) {
  CheckTrackAdapterSettingsEqualsResolution(settings);
  CheckTrackAdapterSettingsEqualsFrameRate(settings);
}

double AspectRatio(const media::VideoCaptureFormat& format) {
  return static_cast<double>(format.frame_size.width()) /
         static_cast<double>(format.frame_size.height());
}

VideoCaptureSettings SelectSettingsVideoDeviceCapture(
    const VideoDeviceCaptureCapabilities& capabilities,
    const MediaConstraints& constraints) {
  return SelectSettingsVideoDeviceCapture(
      capabilities, constraints, MediaStreamVideoSource::kDefaultWidth,
      MediaStreamVideoSource::kDefaultHeight,
      MediaStreamVideoSource::kDefaultFrameRate);
}

}  // namespace

class MediaStreamConstraintsUtilVideoDeviceTest : public testing::Test {
 public:
  void SetUp() override {
    // Default device. It is default because it is the first in the enumeration.
    VideoInputDeviceCapabilities device;
    device.device_id = kDeviceID1;
    device.group_id = kGroupID1;
    device.facing_mode = mojom::blink::FacingMode::kNone;
    device.formats = {
        media::VideoCaptureFormat(gfx::Size(200, 200), 40.0f,
                                  media::PIXEL_FORMAT_I420),
        // This entry is is the closest to defaults.
        media::VideoCaptureFormat(gfx::Size(500, 500), 40.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(1000, 1000), 20.0f,
                                  media::PIXEL_FORMAT_I420),
    };
    device.control_support.pan = false;
    device.control_support.tilt = false;
    device.control_support.zoom = false;
    capabilities_.device_capabilities.push_back(std::move(device));

    // A low-resolution device.
    device.device_id = kDeviceID2;
    device.group_id = kGroupID2;
    device.facing_mode = mojom::blink::FacingMode::kEnvironment;
    device.formats = {
        media::VideoCaptureFormat(gfx::Size(40, 30), 20.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(320, 240), 30.0f,
                                  media::PIXEL_FORMAT_I420),
        // This format has defaults for all settings
        media::VideoCaptureFormat(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight),
            MediaStreamVideoSource::kDefaultFrameRate,
            media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(800, 600), 20.0f,
                                  media::PIXEL_FORMAT_I420),
    };
    device.control_support.pan = true;
    device.control_support.tilt = true;
    device.control_support.zoom = true;
    capabilities_.device_capabilities.push_back(std::move(device));

    // A high-resolution device.
    device.device_id = kDeviceID3;
    device.group_id = kGroupID3;
    device.facing_mode = mojom::blink::FacingMode::kUser;
    device.formats = {
        media::VideoCaptureFormat(gfx::Size(600, 400), 10.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(640, 480), 10.0f,
                                  media::PIXEL_FORMAT_I420),
        // This format has default for all settings, except that the resolution
        // is inverted.
        media::VideoCaptureFormat(
            gfx::Size(MediaStreamVideoSource::kDefaultHeight,
                      MediaStreamVideoSource::kDefaultWidth),
            MediaStreamVideoSource::kDefaultFrameRate,
            media::PIXEL_FORMAT_I420),
        // This format has defaults for all settings
        media::VideoCaptureFormat(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight),
            MediaStreamVideoSource::kDefaultFrameRate,
            media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(1280, 720), 60.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(1920, 1080), 60.0f,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(2304, 1536), 10.0f,
                                  media::PIXEL_FORMAT_I420),
    };
    device.control_support.pan = true;
    device.control_support.tilt = true;
    device.control_support.zoom = true;
    capabilities_.device_capabilities.push_back(std::move(device));

    // A depth capture device.
    device.device_id = kDeviceID4;
    device.group_id = kGroupID4;
    device.facing_mode = mojom::blink::FacingMode::kEnvironment;
    device.formats = {media::VideoCaptureFormat(gfx::Size(640, 480), 30.0f,
                                                media::PIXEL_FORMAT_Y16)};
    device.control_support.pan = true;
    device.control_support.tilt = true;
    device.control_support.zoom = true;
    capabilities_.device_capabilities.push_back(std::move(device));

    // A device that reports invalid frame rates. These devices exist and should
    // be supported if no constraints are placed on the frame rate.
    device.device_id = kDeviceID5;
    device.group_id = kGroupID5;
    device.facing_mode = mojom::blink::FacingMode::kNone;
    device.formats = {
        media::VideoCaptureFormat(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight),
            0.0f, media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(500, 500), 0.1f,
                                  media::PIXEL_FORMAT_I420),
    };
    device.control_support.pan = true;
    device.control_support.tilt = true;
    device.control_support.zoom = true;
    capabilities_.device_capabilities.push_back(std::move(device));

    capabilities_.noise_reduction_capabilities = {
        std::optional<bool>(),
        std::optional<bool>(true),
        std::optional<bool>(false),
    };

    default_device_ = &capabilities_.device_capabilities[0];
    low_res_device_ = &capabilities_.device_capabilities[1];
    high_res_device_ = &capabilities_.device_capabilities[2];
    invalid_frame_rate_device_ = &capabilities_.device_capabilities[4];
    default_closest_format_ = &default_device_->formats[1];
    low_res_closest_format_ = &low_res_device_->formats[2];
    high_res_closest_format_ = &high_res_device_->formats[3];
    high_res_highest_format_ = &high_res_device_->formats[6];
  }

 protected:
  VideoCaptureSettings SelectSettings() {
    MediaConstraints constraints = constraint_factory_.CreateMediaConstraints();
    return SelectSettingsVideoDeviceCapture(capabilities_, constraints);
  }

  base::expected<Vector<VideoCaptureSettings>, std::string>
  SelectEligibleSettings() {
    MediaConstraints constraints = constraint_factory_.CreateMediaConstraints();
    return SelectEligibleSettingsVideoDeviceCapture(
        capabilities_, constraints, MediaStreamVideoSource::kDefaultWidth,
        MediaStreamVideoSource::kDefaultHeight,
        MediaStreamVideoSource::kDefaultFrameRate);
  }

  static WTF::Vector<BooleanConstraint MediaTrackConstraintSetPlatform::*>
  BooleanImageCaptureConstraints() {
    return {
        &MediaTrackConstraintSetPlatform::torch,
        &MediaTrackConstraintSetPlatform::background_blur,
        &MediaTrackConstraintSetPlatform::background_segmentation_mask,
        &MediaTrackConstraintSetPlatform::eye_gaze_correction,
        &MediaTrackConstraintSetPlatform::face_framing,
    };
  }

  static WTF::Vector<DoubleConstraint MediaTrackConstraintSetPlatform::*>
  DoubleImageCaptureConstraints() {
    return {
        &MediaTrackConstraintSetPlatform::exposure_compensation,
        &MediaTrackConstraintSetPlatform::exposure_time,
        &MediaTrackConstraintSetPlatform::color_temperature,
        &MediaTrackConstraintSetPlatform::iso,
        &MediaTrackConstraintSetPlatform::brightness,
        &MediaTrackConstraintSetPlatform::contrast,
        &MediaTrackConstraintSetPlatform::saturation,
        &MediaTrackConstraintSetPlatform::sharpness,
        &MediaTrackConstraintSetPlatform::focus_distance,
    };
  }

  static WTF::Vector<DoubleConstraint MediaTrackConstraintSetPlatform::*>
  PanTiltZoomConstraints() {
    return {
        &MediaTrackConstraintSetPlatform::pan,
        &MediaTrackConstraintSetPlatform::tilt,
        &MediaTrackConstraintSetPlatform::zoom,
    };
  }

  test::TaskEnvironment task_environment_;
  VideoDeviceCaptureCapabilities capabilities_;
  raw_ptr<const VideoInputDeviceCapabilities> default_device_;
  raw_ptr<const VideoInputDeviceCapabilities> low_res_device_;
  raw_ptr<const VideoInputDeviceCapabilities> high_res_device_;
  raw_ptr<const VideoInputDeviceCapabilities> invalid_frame_rate_device_;
  // Closest formats to the default settings.
  raw_ptr<const media::VideoCaptureFormat> default_closest_format_;
  raw_ptr<const media::VideoCaptureFormat> low_res_closest_format_;
  raw_ptr<const media::VideoCaptureFormat> high_res_closest_format_;
  raw_ptr<const media::VideoCaptureFormat> high_res_highest_format_;

  MockConstraintFactory constraint_factory_;
};

// The Unconstrained test checks the default selection criteria.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, Unconstrained) {
  constraint_factory_.Reset();
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Should select the default device with closest-to-default settings.
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*default_closest_format_, result.Format());
  // Should select default settings for other constraints.
  EXPECT_EQ(std::optional<bool>(), result.noise_reduction());
}

// The "Overconstrained" tests verify that failure of any single required
// constraint results in failure to select a candidate.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnDeviceID) {
  constraint_factory_.Reset();
  constraint_factory_.basic().device_id.SetExact("NONEXISTING");
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().device_id.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnGroupID) {
  constraint_factory_.Reset();
  constraint_factory_.basic().group_id.SetExact("NONEXISTING");
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().group_id.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnFacingMode) {
  constraint_factory_.Reset();
  // No device in |capabilities_| has facing mode equal to LEFT.
  constraint_factory_.basic().facing_mode.SetExact("left");
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().facing_mode.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       OverconstrainedOnEmptyFacingMode) {
  constraint_factory_.Reset();
  // Empty is not a valid facingMode value.
  constraint_factory_.basic().facing_mode.SetExact("");
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().facing_mode.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       OverconstrainedOnInvalidResizeMode) {
  constraint_factory_.Reset();
  constraint_factory_.basic().resize_mode.SetExact("invalid");
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().resize_mode.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       OverconstrainedOnEmptyResizeMode) {
  constraint_factory_.Reset();
  constraint_factory_.basic().resize_mode.SetExact("");
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().resize_mode.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnHeight) {
  constraint_factory_.Reset();
  constraint_factory_.basic().height.SetExact(123467890);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().height.SetMin(123467890);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().height.SetMax(0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().height.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnWidth) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetExact(123467890);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetMin(123467890);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetMax(0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       OverconstrainedOnAspectRatio) {
  constraint_factory_.Reset();
  constraint_factory_.basic().aspect_ratio.SetExact(123467890.0);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspect_ratio.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().aspect_ratio.SetMin(123467890.0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspect_ratio.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  // This value is lower than the minimum supported by the test devices.
  const double kLowAspectRatio = 0.00001;
  constraint_factory_.basic().aspect_ratio.SetMax(kLowAspectRatio);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspect_ratio.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, OverconstrainedOnFrameRate) {
  constraint_factory_.Reset();
  constraint_factory_.basic().frame_rate.SetExact(123467890.0);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frame_rate.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().frame_rate.SetMin(123467890.0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frame_rate.GetName(),
            result.failed_constraint_name());

  constraint_factory_.Reset();
  constraint_factory_.basic().frame_rate.SetMax(0.0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().frame_rate.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       OverconstrainedOnNoiseReduction) {
  // Simulate a system that does not support noise reduction.
  // Manually adding device capabilities because VideoDeviceCaptureCapabilities
  // is move only.
  VideoDeviceCaptureCapabilities capabilities;
  VideoInputDeviceCapabilities device;
  device.device_id = kDeviceID1;
  device.facing_mode = mojom::blink::FacingMode::kNone;
  device.formats = {
      media::VideoCaptureFormat(gfx::Size(200, 200), 40.0f,
                                media::PIXEL_FORMAT_I420),
  };
  capabilities.device_capabilities.push_back(std::move(device));
  capabilities.noise_reduction_capabilities = {std::optional<bool>(false)};

  constraint_factory_.Reset();
  constraint_factory_.basic().goog_noise_reduction.SetExact(true);
  auto constraints = constraint_factory_.CreateMediaConstraints();
  auto result = SelectSettingsVideoDeviceCapture(capabilities, constraints);
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().goog_noise_reduction.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       OverconstrainedOnMandatoryPanTiltZoom) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    constraint_factory_.basic().device_id.SetExact(default_device_->device_id);
    (constraint_factory_.basic().*constraint).SetMin(1);
    auto result = SelectSettings();
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(constraint_factory_.basic().device_id.GetName(),
              result.failed_constraint_name());

    constraint_factory_.Reset();
    constraint_factory_.basic().device_id.SetExact(default_device_->device_id);
    (constraint_factory_.basic().*constraint).SetMax(1);
    result = SelectSettings();
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(constraint_factory_.basic().device_id.GetName(),
              result.failed_constraint_name());

    constraint_factory_.Reset();
    constraint_factory_.basic().device_id.SetExact(default_device_->device_id);
    (constraint_factory_.basic().*constraint).SetExact(1);
    result = SelectSettings();
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(constraint_factory_.basic().device_id.GetName(),
              result.failed_constraint_name());
  }
}

// The "Mandatory" and "Ideal" tests check that various selection criteria work
// for each individual constraint in the basic constraint set.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryDeviceID) {
  constraint_factory_.Reset();
  constraint_factory_.basic().device_id.SetExact(default_device_->device_id);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*default_closest_format_, result.Format());
  CheckTrackAdapterSettingsEqualsFormat(result);

  constraint_factory_.basic().device_id.SetExact(low_res_device_->device_id);
  result = SelectSettings();
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*low_res_closest_format_, result.Format());
  CheckTrackAdapterSettingsEqualsFormat(result);

  constraint_factory_.basic().device_id.SetExact(high_res_device_->device_id);
  result = SelectSettings();
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*high_res_closest_format_, result.Format());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryGroupID) {
  constraint_factory_.Reset();
  constraint_factory_.basic().group_id.SetExact(default_device_->group_id);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*default_closest_format_, result.Format());
  CheckTrackAdapterSettingsEqualsFormat(result);

  constraint_factory_.basic().group_id.SetExact(low_res_device_->group_id);
  result = SelectSettings();
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*low_res_closest_format_, result.Format());
  CheckTrackAdapterSettingsEqualsFormat(result);

  constraint_factory_.basic().group_id.SetExact(high_res_device_->group_id);
  result = SelectSettings();
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*high_res_closest_format_, result.Format());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryFacingMode) {
  constraint_factory_.Reset();
  constraint_factory_.basic().facing_mode.SetExact("environment");
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the low-res device supports kEnvironment facing mode. Should select
  // default settings for everything else.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(mojom::blink::FacingMode::kEnvironment,
            low_res_device_->facing_mode);
  EXPECT_EQ(*low_res_closest_format_, result.Format());
  CheckTrackAdapterSettingsEqualsFormat(result);

  constraint_factory_.basic().facing_mode.SetExact("user");
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device supports kUser facing mode. Should select default
  // settings for everything else.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(mojom::blink::FacingMode::kUser, high_res_device_->facing_mode);
  EXPECT_EQ(*high_res_closest_format_, result.Format());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryNoiseReduction) {
  constraint_factory_.Reset();
  const bool kNoiseReductionValues[] = {true, false};
  for (auto noise_reduction : kNoiseReductionValues) {
    constraint_factory_.basic().goog_noise_reduction.SetExact(noise_reduction);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(noise_reduction, result.noise_reduction());
    // The default device and settings closest to the default should be
    // selected.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryExactHeight) {
  constraint_factory_.Reset();
  const int kHeight = MediaStreamVideoSource::kDefaultHeight;
  constraint_factory_.basic().height.SetExact(kHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested height. The algorithm
  // should prefer the first device that supports the requested height natively,
  // which is the low-res device.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(kHeight, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());

  const int kLargeHeight = 1500;
  constraint_factory_.basic().height.SetExact(kLargeHeight);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device at the highest resolution supports the requested
  // height, even if not natively.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*high_res_highest_format_, result.Format());
  EXPECT_EQ(kLargeHeight, result.track_adapter_settings().target_height());
  EXPECT_EQ(std::round(kLargeHeight * AspectRatio(*high_res_highest_format_)),
            result.track_adapter_settings().target_width());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMinHeight) {
  constraint_factory_.Reset();
  const int kHeight = MediaStreamVideoSource::kDefaultHeight;
  constraint_factory_.basic().height.SetMin(kHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested height range. The
  // algorithm should prefer the default device.
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_LE(kHeight, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(static_cast<double>(result.Width()) / kHeight,
            result.track_adapter_settings().max_aspect_ratio());
  EXPECT_EQ(1.0 / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);

  const int kLargeHeight = 1500;
  constraint_factory_.basic().height.SetMin(kLargeHeight);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device at the highest resolution supports the requested
  // height range.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*high_res_highest_format_, result.Format());
  EXPECT_LE(kHeight, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(static_cast<double>(result.Width()) / kLargeHeight,
            result.track_adapter_settings().max_aspect_ratio());
  EXPECT_EQ(1.0 / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMaxHeight) {
  constraint_factory_.Reset();
  const int kLowHeight = 20;
  constraint_factory_.basic().height.SetMax(kLowHeight);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested height range. The
  // algorithm should prefer the settings that natively exceed the requested
  // maximum by the lowest amount. In this case it is the low-res device.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(low_res_device_->formats[0], result.Format());
  EXPECT_EQ(kLowHeight, result.track_adapter_settings().target_height());
  EXPECT_EQ(std::round(kLowHeight * AspectRatio(result.Format())),
            result.track_adapter_settings().target_width());
  EXPECT_EQ(static_cast<double>(result.Width()),
            result.track_adapter_settings().max_aspect_ratio());
  EXPECT_EQ(1.0 / kLowHeight,
            result.track_adapter_settings().min_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryHeightRange) {
  constraint_factory_.Reset();
  {
    const int kMinHeight = 480;
    const int kMaxHeight = 720;
    constraint_factory_.basic().height.SetMin(kMinHeight);
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Height(), kMinHeight);
    EXPECT_LE(result.Height(), kMaxHeight);
    // All devices in |capabilities_| support the constraint range. The
    // algorithm should prefer the default device since it has at least one
    // native format (the closest-to-default format) included in the requested
    // range.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(static_cast<double>(result.Width()) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const int kMinHeight = 550;
    const int kMaxHeight = 650;
    constraint_factory_.basic().height.SetMin(kMinHeight);
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Height(), kMinHeight);
    EXPECT_LE(result.Height(), kMaxHeight);
    // In this case, the algorithm should prefer the low-res device since it is
    // the first device with a native format (800x600) included in the requested
    // range.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(800, result.Width());
    EXPECT_EQ(600, result.Height());
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(static_cast<double>(result.Width()) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const int kMinHeight = 700;
    const int kMaxHeight = 800;
    constraint_factory_.basic().height.SetMin(kMinHeight);
    constraint_factory_.basic().height.SetMax(kMaxHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Height(), kMinHeight);
    EXPECT_LE(result.Height(), kMaxHeight);
    // In this case, the algorithm should prefer the high-res device since it is
    // the only device with a native format (1280x720) included in the requested
    // range.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(static_cast<double>(result.Width()) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealHeight) {
  constraint_factory_.Reset();
  {
    const int kIdealHeight = 480;
    constraint_factory_.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm should select the first device that supports the ideal
    // height natively.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(kIdealHeight, result.Height());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }

  {
    const int kIdealHeight = 481;
    constraint_factory_.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // In this case, the default device is selected because it can satisfy the
    // ideal at a lower cost than the other devices (500 vs 600 or 720).
    // Note that a native resolution of 480 is further from the ideal than
    // 500 cropped to 480.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    // The track is cropped to the ideal height, maintaining the source aspect
    // ratio.
    EXPECT_EQ(kIdealHeight, result.track_adapter_settings().target_height());
    EXPECT_EQ(std::round(kIdealHeight * AspectRatio(result.Format())),
              result.track_adapter_settings().target_width());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const int kIdealHeight = 1079;
    constraint_factory_.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // In this case, the high-res device has two configurations that satisfy
    // the ideal value (1920x1080 and 2304x1536). Select the one with shortest
    // native distance to the ideal value (1920x1080).
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    EXPECT_EQ(kIdealHeight, result.track_adapter_settings().target_height());
    EXPECT_EQ(std::round(kIdealHeight * AspectRatio(result.Format())),
              result.track_adapter_settings().target_width());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const int kIdealHeight = 1200;
    constraint_factory_.basic().height.SetIdeal(kIdealHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm must the select the only device that can satisfy the ideal,
    // which is the high-res device at the highest resolution.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*high_res_highest_format_, result.Format());
    EXPECT_EQ(kIdealHeight, result.track_adapter_settings().target_height());
    EXPECT_EQ(std::round(kIdealHeight * AspectRatio(result.Format())),
              result.track_adapter_settings().target_width());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryExactWidth) {
  constraint_factory_.Reset();
  const int kWidth = 640;
  constraint_factory_.basic().width.SetExact(kWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested width. The algorithm
  // should prefer the first device that supports the requested width natively,
  // which is the low-res device.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(kWidth, result.Width());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(kWidth, result.track_adapter_settings().max_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kWidth) / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);

  const int kLargeWidth = 2000;
  constraint_factory_.basic().width.SetExact(kLargeWidth);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_LE(kLargeWidth, result.Width());
  // Only the high-res device at the highest resolution supports the requested
  // width, even if not natively.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*high_res_highest_format_, result.Format());
  EXPECT_EQ(std::round(kLargeWidth / AspectRatio(result.Format())),
            result.track_adapter_settings().target_height());
  EXPECT_EQ(kLargeWidth, result.track_adapter_settings().target_width());
  EXPECT_EQ(kLargeWidth, result.track_adapter_settings().max_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kLargeWidth) / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMinWidth) {
  constraint_factory_.Reset();
  const int kWidth = 640;
  constraint_factory_.basic().width.SetMin(kWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested width range. The
  // algorithm should prefer the default device at 1000x1000, which is the
  // first configuration that satisfies the minimum width.
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_LE(kWidth, result.Width());
  EXPECT_EQ(1000, result.Width());
  EXPECT_EQ(1000, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(result.Width(), result.track_adapter_settings().max_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kWidth) / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);

  const int kLargeWidth = 2000;
  constraint_factory_.basic().width.SetMin(kLargeWidth);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device at the highest resolution supports the requested
  // minimum width.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_LE(kLargeWidth, result.Width());
  EXPECT_EQ(*high_res_highest_format_, result.Format());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(result.Width(), result.track_adapter_settings().max_aspect_ratio());
  EXPECT_EQ(static_cast<double>(kLargeWidth) / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMaxWidth) {
  constraint_factory_.Reset();
  const int kLowWidth = 30;
  constraint_factory_.basic().width.SetMax(kLowWidth);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested width range. The
  // algorithm should prefer the settings that natively exceed the requested
  // maximum by the lowest amount. In this case it is the low-res device at its
  // lowest resolution.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(low_res_device_->formats[0], result.Format());
  // The track is cropped to kLowWidth and keeps the source aspect ratio.
  EXPECT_EQ(std::round(kLowWidth / AspectRatio(result.Format())),
            result.track_adapter_settings().target_height());
  EXPECT_EQ(kLowWidth, result.track_adapter_settings().target_width());
  EXPECT_EQ(kLowWidth, result.track_adapter_settings().max_aspect_ratio());
  EXPECT_EQ(1.0 / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryWidthRange) {
  constraint_factory_.Reset();
  {
    const int kMinWidth = 640;
    const int kMaxWidth = 1280;
    constraint_factory_.basic().width.SetMin(kMinWidth);
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Width(), kMinWidth);
    EXPECT_LE(result.Width(), kMaxWidth);
    // All devices in |capabilities_| support the constraint range. The
    // algorithm should prefer the default device since it has at least one
    // native format (1000x1000) included in the requested range.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1000, result.Width());
    EXPECT_EQ(1000, result.Height());
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMinWidth) / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const int kMinWidth = 750;
    const int kMaxWidth = 850;
    constraint_factory_.basic().width.SetMin(kMinWidth);
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Width(), kMinWidth);
    EXPECT_LE(result.Width(), kMaxWidth);
    // In this case, the algorithm should prefer the low-res device since it is
    // the first device with a native format (800x600) included in the requested
    // range.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(800, result.Width());
    EXPECT_EQ(600, result.Height());
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMinWidth) / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const int kMinWidth = 1900;
    const int kMaxWidth = 2000;
    constraint_factory_.basic().width.SetMin(kMinWidth);
    constraint_factory_.basic().width.SetMax(kMaxWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.Width(), kMinWidth);
    EXPECT_LE(result.Width(), kMaxWidth);
    // In this case, the algorithm should prefer the high-res device since it is
    // the only device with a native format (1920x1080) included in the
    // requested range.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(static_cast<double>(kMinWidth) / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealWidth) {
  constraint_factory_.Reset();
  {
    const int kIdealWidth = 320;
    constraint_factory_.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm should select the first device that supports the ideal
    // width natively, which is the low-res device at 320x240.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(kIdealWidth, result.Width());
    // The ideal value is satisfied with a native resolution, so no rescaling.
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(kIdealWidth, result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const int kIdealWidth = 321;
    constraint_factory_.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // In this case, the high_res device is selected because it has a mode that
    // can satisfy the ideal at a lower cost than other devices (480 vs 500).
    // Note that a native resolution of 320 is further from the ideal value of
    // 321 than 480 cropped to 321.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(480, result.Width());
    // The track is cropped to kIdealWidth and keeps the source aspect ratio.
    EXPECT_EQ(std::round(kIdealWidth / AspectRatio(result.Format())),
              result.track_adapter_settings().target_height());
    EXPECT_EQ(kIdealWidth, result.track_adapter_settings().target_width());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const int kIdealWidth = 2000;
    constraint_factory_.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm must the select the only device that can satisfy the ideal.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*high_res_highest_format_, result.Format());
    // The track is cropped to kIdealWidth and keeps the source aspect ratio.
    EXPECT_EQ(std::round(kIdealWidth / AspectRatio(result.Format())),
              result.track_adapter_settings().target_height());
    EXPECT_EQ(kIdealWidth, result.track_adapter_settings().target_width());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const int kIdealWidth = 3000;
    constraint_factory_.basic().width.SetIdeal(kIdealWidth);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm must the select the device and setting with less distance
    // to the ideal.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*high_res_highest_format_, result.Format());
    CheckTrackAdapterSettingsEqualsFormat(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryExactFrameRate) {
  constraint_factory_.Reset();
  const double kFrameRate = MediaStreamVideoSource::kDefaultFrameRate;
  constraint_factory_.basic().frame_rate.SetExact(kFrameRate);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // All devices in |capabilities_| support the requested frame rate. The
  // algorithm should prefer the first device that supports the requested frame
  // rate natively, which is the low-res device at 640x480x30Hz.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(kFrameRate, result.FrameRate());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  CheckTrackAdapterSettingsEqualsResolution(result);
  CheckTrackAdapterSettingsEqualsFrameRate(result, kFrameRate);

  const double kLargeFrameRate = 50;
  constraint_factory_.basic().frame_rate.SetExact(kLargeFrameRate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Only the high-res device supports the requested frame rate, even if not
  // natively. The least expensive configuration that supports the requested
  // frame rate is 1280x720x60Hz.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(60.0, result.FrameRate());
  EXPECT_EQ(1280, result.Width());
  EXPECT_EQ(720, result.Height());
  CheckTrackAdapterSettingsEqualsResolution(result);
  CheckTrackAdapterSettingsEqualsFrameRate(result, kLargeFrameRate);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMinFrameRate) {
  // MinFrameRate equal to default frame rate.
  {
    constraint_factory_.Reset();
    const double kMinFrameRate = MediaStreamVideoSource::kDefaultFrameRate;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // All devices in |capabilities_| support the requested frame-rate range.
    // The algorithm should prefer the default device.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    // The format closest to the default satisfies the constraint.
    EXPECT_EQ(*default_closest_format_, result.Format());
    CheckTrackAdapterSettingsEqualsFormat(result);
    EXPECT_TRUE(result.min_frame_rate().has_value());
    EXPECT_EQ(result.min_frame_rate(), kMinFrameRate);
    EXPECT_FALSE(result.max_frame_rate().has_value());
  }

  // MinFrameRate greater than default frame rate.
  {
    const double kMinFrameRate = 50;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Only the high-res device supports the requested frame-rate range.
    // The least expensive configuration is 1280x720x60Hz.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_LE(kMinFrameRate, result.FrameRate());
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
    CheckTrackAdapterSettingsEqualsFormat(result);
    EXPECT_TRUE(result.min_frame_rate().has_value());
    EXPECT_EQ(result.min_frame_rate(), kMinFrameRate);
    EXPECT_FALSE(result.max_frame_rate().has_value());
  }

  // MinFrameRate lower than the minimum allowed value.
  {
    const double kMinFrameRate = -0.01;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The minimum frame rate is ignored. Default settings should be used.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    EXPECT_FALSE(result.min_frame_rate().has_value());
    EXPECT_FALSE(result.max_frame_rate().has_value());
  }

  // MinFrameRate equal to the minimum allowed value.
  {
    const double kMinFrameRate = 0.0;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_TRUE(result.min_frame_rate().has_value());
    EXPECT_EQ(result.min_frame_rate(), kMinFrameRate);
    EXPECT_FALSE(result.max_frame_rate().has_value());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMaxFrameRate) {
  // MaxFrameRate within valid range.
  {
    constraint_factory_.Reset();
    const double kMaxFrameRate = 10;
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // All devices in |capabilities_| support the requested frame-rate range.
    // The algorithm should prefer the settings that natively exceed the
    // requested maximum by the lowest amount. In this case it is the high-res
    // device with default resolution .
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(kMaxFrameRate, result.FrameRate());
    EXPECT_EQ(MediaStreamVideoSource::kDefaultHeight, result.Height());
    EXPECT_EQ(MediaStreamVideoSource::kDefaultWidth, result.Width());
    EXPECT_FALSE(result.min_frame_rate().has_value());
    EXPECT_TRUE(result.max_frame_rate().has_value());
    EXPECT_EQ(kMaxFrameRate, result.max_frame_rate());
    CheckTrackAdapterSettingsEqualsResolution(result);
    CheckTrackAdapterSettingsEqualsFrameRate(result, kMaxFrameRate);
  }

  // MaxFrameRate greater than the maximum allowed.
  {
    constraint_factory_.Reset();
    const double kMaxFrameRate =
        static_cast<double>(media::limits::kMaxFramesPerSecond) + 0.1;
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The maximum frame rate should be ignored. Default settings apply.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    EXPECT_FALSE(result.min_frame_rate().has_value());
    EXPECT_FALSE(result.max_frame_rate().has_value());
  }

  // MaxFrameRate equal to the maximum and minimum allowed MaxFrameRate.
  {
    const double kMaxFrameRates[] = {1.0, media::limits::kMaxFramesPerSecond};
    for (double max_frame_rate : kMaxFrameRates) {
      constraint_factory_.Reset();
      constraint_factory_.basic().frame_rate.SetMax(max_frame_rate);
      auto result = SelectSettings();
      EXPECT_TRUE(result.HasValue());
      EXPECT_TRUE(result.max_frame_rate().has_value());
      EXPECT_FALSE(result.min_frame_rate().has_value());
      EXPECT_EQ(result.max_frame_rate(), max_frame_rate);
    }
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryFrameRateRange) {
  constraint_factory_.Reset();
  {
    const double kMinFrameRate = 10;
    const double kMaxFrameRate = 40;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_LE(kMinFrameRate, result.FrameRate());
    EXPECT_GE(kMaxFrameRate, result.FrameRate());
    // All devices in |capabilities_| support the constraint range. The
    // algorithm should prefer the default device since its closest-to-default
    // format has a frame rate included in the requested range.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    CheckTrackAdapterSettingsEqualsResolution(result);
    CheckTrackAdapterSettingsEqualsFrameRate(result, kMaxFrameRate);
  }

  {
    const double kMinFrameRate = 25;
    const double kMaxFrameRate = 35;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.FrameRate(), kMinFrameRate);
    EXPECT_LE(result.FrameRate(), kMaxFrameRate);
    // In this case, the algorithm should prefer the low-res device since it is
    // the first device with a native frame rate included in the requested
    // range. The default resolution should be preferred as secondary criterion.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*low_res_closest_format_, result.Format());
    CheckTrackAdapterSettingsEqualsResolution(result);
    CheckTrackAdapterSettingsEqualsFrameRate(result, kMaxFrameRate);
  }

  {
    const double kMinFrameRate = 50;
    const double kMaxFrameRate = 70;
    constraint_factory_.basic().frame_rate.SetMin(kMinFrameRate);
    constraint_factory_.basic().frame_rate.SetMax(kMaxFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_GE(result.FrameRate(), kMinFrameRate);
    EXPECT_LE(result.FrameRate(), kMaxFrameRate);
    // In this case, the algorithm should prefer the high-res device since it is
    // the only device with a native format included in the requested range.
    // The 1280x720 resolution should be selected due to closeness to default
    // settings, which is the second tie-breaker criterion that applies.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
    CheckTrackAdapterSettingsEqualsResolution(result);
    CheckTrackAdapterSettingsEqualsFrameRate(result, kMaxFrameRate);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealFrameRate) {
  constraint_factory_.Reset();
  {
    const double kIdealFrameRate = MediaStreamVideoSource::kDefaultFrameRate;
    constraint_factory_.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm should select the first configuration that supports the
    // ideal frame rate natively, which is the low-res device. Default
    // resolution should be selected as secondary criterion.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*low_res_closest_format_, result.Format());
    CheckTrackAdapterSettingsEqualsResolution(result);
    CheckTrackAdapterSettingsEqualsFrameRate(result, kIdealFrameRate);
  }

  {
    const double kIdealFrameRate = 31;
    constraint_factory_.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // In this case, the default device is selected because it can satisfy the
    // ideal at a lower cost than the other devices (40 vs 60).
    // Note that a native frame rate of 30 is further from the ideal than
    // 31 adjusted to 30.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    CheckTrackAdapterSettingsEqualsResolution(result);
    CheckTrackAdapterSettingsEqualsFrameRate(result, kIdealFrameRate);
  }

  {
    const double kIdealFrameRate = 55;
    constraint_factory_.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The high-res device format 1280x720x60.0 must be selected because its
    // frame rate can satisfy the ideal frame rate and has resolution closest
    // to the default.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
    EXPECT_EQ(60, result.FrameRate());
    CheckTrackAdapterSettingsEqualsResolution(result);
    CheckTrackAdapterSettingsEqualsFrameRate(result, kIdealFrameRate);
  }

  {
    const double kIdealFrameRate = 100;
    constraint_factory_.basic().frame_rate.SetIdeal(kIdealFrameRate);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm must select settings with frame rate closest to the ideal.
    // The high-res device format 1280x720x60.0 must be selected because its
    // frame rate it closest to the ideal value and it has resolution closest to
    // the default.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
    EXPECT_EQ(60, result.FrameRate());
    CheckTrackAdapterSettingsEqualsResolution(result);
    CheckTrackAdapterSettingsEqualsFrameRate(result, kIdealFrameRate);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryExactAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 4.0 / 3.0;
  constraint_factory_.basic().aspect_ratio.SetExact(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  double min_width = 1.0;
  double max_width = result.Width();
  double min_height = 1.0;
  double max_height = result.Height();
  double min_aspect_ratio = min_width / max_height;
  double max_aspect_ratio = max_width / min_height;
  // The requested aspect ratio must be within the supported range.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // All devices in |capabilities_| support the requested aspect ratio.
  // The algorithm should prefer the first device that supports the requested
  // aspect ratio.
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*default_closest_format_, result.Format());
  EXPECT_EQ(std::round(result.Width() / kAspectRatio),
            result.track_adapter_settings().target_height());
  EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);

  const int kMinWidth = 500;
  const int kMaxWidth = 1000;
  const int kMaxHeight = 500;
  constraint_factory_.basic().height.SetMax(kMaxHeight);
  constraint_factory_.basic().width.SetMin(kMinWidth);
  constraint_factory_.basic().width.SetMax(kMaxWidth);
  constraint_factory_.basic().aspect_ratio.SetExact(kAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  min_width = std::max(1, kMinWidth);
  max_width = std::min(result.Width(), kMaxWidth);
  min_height = 1.0;
  max_height = std::min(result.Height(), kMaxHeight);
  min_aspect_ratio = min_width / max_height;
  max_aspect_ratio = max_width / min_height;
  // The requested aspect ratio must be within the supported range.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // The default device can support the requested aspect ratio with the default
  // settings (500x500) using cropping.
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*default_closest_format_, result.Format());
  EXPECT_EQ(std::round(result.Width() / kAspectRatio),
            result.track_adapter_settings().target_height());
  EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);

  const int kMinHeight = 480;
  constraint_factory_.basic().height.SetMin(kMinHeight);
  constraint_factory_.basic().height.SetMax(kMaxHeight);
  constraint_factory_.basic().width.SetMin(kMinWidth);
  constraint_factory_.basic().width.SetMax(kMaxWidth);
  constraint_factory_.basic().aspect_ratio.SetExact(kAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  min_width = std::max(1, kMinWidth);
  max_width = std::min(result.Width(), kMaxWidth);
  min_height = std::max(1, kMinHeight);
  max_height = std::min(result.Height(), kMaxHeight);
  min_aspect_ratio = min_width / max_height;
  max_aspect_ratio = max_width / min_height;
  // The requested aspect ratio must be within the supported range.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // Given resolution constraints, the default device with closest-to-default
  // settings cannot satisfy the required aspect ratio.
  // The first device that can do it is the low-res device with a native
  // resolution of 640x480. Higher resolutions for the default device are more
  // penalized by the constraints than the default native resolution of the
  // low-res device.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*low_res_closest_format_, result.Format());
  // Native resolution, so no rescaling.
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMinAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 4.0 / 3.0;
  constraint_factory_.basic().aspect_ratio.SetMin(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  double max_width = result.Width();
  double min_height = 1.0;
  double max_aspect_ratio = max_width / min_height;
  // Minimum constraint aspect ratio must be less than or equal to the maximum
  // supported by the source.
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // All devices in |capabilities_| support the requested aspect-ratio range.
  // The algorithm should prefer the first device that supports the requested
  // aspect-ratio range, which in this case is the default device.
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*default_closest_format_, result.Format());
  // Adjust the track resolution to use the minimum aspect ratio, which is
  // greater than the source's aspect ratio.
  EXPECT_EQ(std::round(result.Width() / kAspectRatio),
            result.track_adapter_settings().target_height());
  EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(result.Width(), result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);

  const int kMinWidth = 500;
  const int kMaxWidth = 1000;
  const int kMinHeight = 480;
  const int kMaxHeight = 500;
  constraint_factory_.basic().width.SetMin(kMinWidth);
  constraint_factory_.basic().width.SetMax(kMaxWidth);
  constraint_factory_.basic().height.SetMin(kMinHeight);
  constraint_factory_.basic().height.SetMax(kMaxHeight);
  constraint_factory_.basic().aspect_ratio.SetMin(kAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  max_width = std::min(result.Width(), kMaxWidth);
  min_height = std::max(1, kMinHeight);
  max_aspect_ratio = max_width / min_height;
  // Minimum constraint aspect ratio must be less than or equal to the minimum
  // supported by the source.
  EXPECT_LE(kAspectRatio, max_aspect_ratio);
  // Given resolution constraints, the default device with closest-to-default
  // settings cannot satisfy the required minimum aspect ratio (maximum would
  // be 500/480).  The first device that can is the low-res device with a native
  // resolution of 640x480.
  // Higher resolutions for the default device are more penalized by the
  // constraints than the default native resolution of the low-res device.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*low_res_closest_format_, result.Format());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  // The source's native aspect ratio equals the minimum aspect ratio.
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(max_aspect_ratio,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMaxAspectRatio) {
  constraint_factory_.Reset();
  const double kAspectRatio = 0.5;
  constraint_factory_.basic().aspect_ratio.SetMax(kAspectRatio);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  double min_width = 1.0;
  double max_height = result.Height();
  double min_aspect_ratio = min_width / max_height;
  // Minimum constraint aspect ratio must be less than or equal to the maximum
  // supported by the source.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  // All devices in |capabilities_| support the requested aspect-ratio range.
  // The algorithm should prefer the first device that supports the requested
  // aspect-ratio range, which in this case is the default device.
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*default_closest_format_, result.Format());
  // The track's aspect ratio is adjusted to the maximum, which is lower than
  // the source's native aspect ratio.
  EXPECT_EQ(result.Height(), result.track_adapter_settings().target_height());
  EXPECT_EQ(std::round(result.Height() * kAspectRatio),
            result.track_adapter_settings().target_width());
  EXPECT_EQ(min_aspect_ratio,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);

  const int kExactWidth = 360;
  const int kMinHeight = 360;
  const int kMaxHeight = 720;
  constraint_factory_.basic().width.SetExact(kExactWidth);
  constraint_factory_.basic().height.SetMin(kMinHeight);
  constraint_factory_.basic().height.SetMax(kMaxHeight);
  constraint_factory_.basic().aspect_ratio.SetMax(kAspectRatio);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  min_width = std::max(1, kExactWidth);
  max_height = std::min(result.Height(), kMaxHeight);
  min_aspect_ratio = min_width / max_height;
  // Minimum constraint aspect ratio must be less than or equal to the minimum
  // supported by the source.
  EXPECT_GE(kAspectRatio, min_aspect_ratio);
  // Given resolution constraints, the default device with closest-to-default
  // settings cannot satisfy the required maximum aspect ratio (maximum would
  // be 360/500).
  // The high-res device with a native resolution of 1280x720 can support
  // 360x720 with cropping with less penalty than the default device at
  // 1000x1000.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(1280, result.Width());
  EXPECT_EQ(720, result.Height());
  // The track's aspect ratio is adjusted to the maximum, which is lower than
  // the source's native aspect ratio.
  EXPECT_EQ(result.Height(), result.track_adapter_settings().target_height());
  EXPECT_EQ(std::round(result.Height() * kAspectRatio),
            result.track_adapter_settings().target_width());
  EXPECT_EQ(min_aspect_ratio,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(kAspectRatio, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryAspectRatioRange) {
  constraint_factory_.Reset();
  {
    const double kMinAspectRatio = 0.5;
    const double kMaxAspectRatio = 1.0;

    constraint_factory_.basic().aspect_ratio.SetMin(kMinAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    double min_width = 1.0;
    double max_width = result.Width();
    double min_height = 1.0;
    double max_height = result.Height();
    double min_aspect_ratio = min_width / max_height;
    double max_aspect_ratio = max_width / min_height;
    // Constraint aspect-ratio range must have nonempty intersection with
    // supported range.
    EXPECT_LE(kMinAspectRatio, max_aspect_ratio);
    EXPECT_GE(kMaxAspectRatio, min_aspect_ratio);
    // All devices in |capabilities_| support the requested aspect-ratio range.
    // The algorithm should prefer the first device that supports the requested
    // aspect-ratio range, which in this case is the default device.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    // The source's aspect ratio matches the maximum aspect ratio. No adjustment
    // is required.
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(kMaxAspectRatio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const double kMinAspectRatio = 3.0;
    const double kMaxAspectRatio = 4.0;

    const int kMinHeight = 600;
    constraint_factory_.Reset();
    constraint_factory_.basic().height.SetMin(kMinHeight);
    constraint_factory_.basic().aspect_ratio.SetMin(kMinAspectRatio);
    constraint_factory_.basic().aspect_ratio.SetMax(kMaxAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    double min_width = 1.0;
    double max_width = result.Width();
    double min_height = 1.0;
    double max_height = result.Height();
    double min_aspect_ratio = min_width / max_height;
    double max_aspect_ratio = max_width / min_height;
    // Constraint aspect-ratio range must have nonempty intersection with
    // supported range.
    EXPECT_LE(kMinAspectRatio, max_aspect_ratio);
    EXPECT_GE(kMaxAspectRatio, min_aspect_ratio);
    // The only device that supports the resolution and aspect ratio constraint
    // is the high-res device. The 1920x1080 is the least expensive format.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    // The track is cropped to support the minimum aspect ratio.
    EXPECT_EQ(std::round(result.Width() / kMinAspectRatio),
              result.track_adapter_settings().target_height());
    EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
    EXPECT_EQ(kMinAspectRatio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(result.Width()) / kMinHeight,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealAspectRatio) {
  constraint_factory_.Reset();
  {
    const double kIdealAspectRatio = 0.5;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    double min_width = 1.0;
    double max_width = result.Width();
    double min_height = 1.0;
    double max_height = result.Height();
    double min_aspect_ratio = min_width / max_height;
    double max_aspect_ratio = max_width / min_height;
    // All devices in |capabilities_| support the ideal aspect-ratio.
    // The algorithm should prefer the default device with closest-to-default
    // settings.
    EXPECT_LE(kIdealAspectRatio, max_aspect_ratio);
    EXPECT_GE(kIdealAspectRatio, min_aspect_ratio);
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    // The track is cropped to support the ideal aspect ratio.
    EXPECT_EQ(result.Height(), result.track_adapter_settings().target_height());
    EXPECT_EQ(std::round(result.Height() * kIdealAspectRatio),
              result.track_adapter_settings().target_width());
    EXPECT_EQ(min_aspect_ratio,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(max_aspect_ratio,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const double kIdealAspectRatio = 1500.0;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The only device that supports the ideal aspect ratio is the high-res
    // device.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
    // The most exact way to support the ideal aspect ratio would be to crop to
    // 1920x1080 to 1500x1. However, with 1920x1080 the algorithm tries to crop
    // to 1920x1.28 and rounds to 1920x1. Since the aspect ratio of 1280x1 is
    // closer to ideal than 1920x1, 1280x1 is selected instead.
    // In this case, the effect of rounding is noticeable because of the
    // resulting low value for height. For more typical aspect-ratio values,
    // the 1-pixel error caused by rounding one dimension does not translate to
    // a absolute error on the other dimension.
    EXPECT_EQ(std::round(result.Width() / kIdealAspectRatio),
              result.track_adapter_settings().target_height());
    EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const double kIdealAspectRatio = 2000.0;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The best way to support this ideal aspect ratio would be to rescale
    // 2304x1536 to 2000x1, but the algorithm would try to rescale to 2304x1.15
    // and then round. Since 1920x1 has an aspect ratio closer to 2000, it is
    // selected over 2304x1. The only device that supports this resolution is
    // the high-res device open at 1920x1080.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1920, result.Width());
    EXPECT_EQ(1080, result.Height());
    EXPECT_EQ(std::round(result.Width() / kIdealAspectRatio),
              result.track_adapter_settings().target_height());
    EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const double kIdealAspectRatio = 4000.0;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The configuration closest to the ideal aspect ratio is is the high-res
    // device with its highest resolution, cropped to 2304x1.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*high_res_highest_format_, result.Format());
    // In this case there is no rounding error.
    EXPECT_EQ(1, result.track_adapter_settings().target_height());
    EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
    EXPECT_EQ(1.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const double kIdealAspectRatio = 2.0;
    const int kExactHeight = 400;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    constraint_factory_.basic().height.SetExact(kExactHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The first device to support the ideal aspect ratio and the resolution
    // constraint is the low-res device. The 800x600 format cropped to 800x400
    // is the lest expensive way to achieve it.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(800, result.Width());
    EXPECT_EQ(600, result.Height());
    EXPECT_EQ(kExactHeight, result.track_adapter_settings().target_height());
    EXPECT_EQ(kExactHeight * kIdealAspectRatio,
              result.track_adapter_settings().target_width());
    EXPECT_EQ(1.0 / kExactHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(result.Width()) / kExactHeight,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  {
    const double kIdealAspectRatio = 3.0;
    const int kExactHeight = 400;
    constraint_factory_.basic().aspect_ratio.SetIdeal(kIdealAspectRatio);
    constraint_factory_.basic().height.SetExact(kExactHeight);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The only device that supports the ideal aspect ratio and the resolution
    // constraint is the high-res device. The 1280x720 cropped to 1200x400 is
    // the lest expensive way to achieve it.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(1280, result.Width());
    EXPECT_EQ(720, result.Height());
    EXPECT_EQ(kExactHeight, result.track_adapter_settings().target_height());
    EXPECT_EQ(kExactHeight * kIdealAspectRatio,
              result.track_adapter_settings().target_width());
    EXPECT_EQ(1.0 / kExactHeight,
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(static_cast<double>(result.Width()) / kExactHeight,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryResizeMode) {
  const int kIdealWidth = 641;
  const int kIdealHeight = 480;
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetIdeal(kIdealWidth);
  constraint_factory_.basic().height.SetIdeal(kIdealHeight);
  constraint_factory_.basic().resize_mode.SetExact("none");
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // A native mode of 640x480 should be selected since it is closest native mode
  // to the ideal values.
  EXPECT_EQ(result.Width(), 640);
  EXPECT_EQ(result.Height(), 480);
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());

  constraint_factory_.basic().resize_mode.SetExact("crop-and-scale");
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_GE(result.Width(), kIdealWidth);
  EXPECT_GE(result.Height(), kIdealHeight);
  EXPECT_EQ(result.track_adapter_settings().target_width(), kIdealWidth);
  EXPECT_EQ(result.track_adapter_settings().target_height(), kIdealHeight);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealResizeMode) {
  constraint_factory_.Reset();
  constraint_factory_.basic().resize_mode.SetIdeal("crop-and-scale");
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Since no constraints are given, the default device with resolution closest
  // to default is selected. However, rescaling is enabled due to the ideal
  // resize mode.
  EXPECT_EQ(result.device_id(), default_device_->device_id.Utf8());
  EXPECT_EQ(result.Width(), 500);
  EXPECT_EQ(result.Height(), 500);
  EXPECT_TRUE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(result.track_adapter_settings().target_width(), 500);
  EXPECT_EQ(result.track_adapter_settings().target_height(), 500);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       IdealResizeModeResolutionGreaterThanNative) {
  // Ideal resolution is slightly greater than the closest native resolution.
  const int kIdealWidth = 641;
  const int kIdealHeight = 480;
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetIdeal(kIdealWidth);
  constraint_factory_.basic().height.SetIdeal(kIdealHeight);
  constraint_factory_.basic().resize_mode.SetIdeal(
      WebString::FromASCII("none"));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // A native mode of 640x480 should be selected since it is the closest native
  // mode to the ideal resolution values.
  EXPECT_EQ(result.Width(), 640);
  EXPECT_EQ(result.Height(), 480);
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());

  constraint_factory_.basic().resize_mode.SetIdeal("crop-and-scale");
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_GE(result.Width(), kIdealWidth);
  EXPECT_GE(result.Height(), kIdealHeight);
  EXPECT_EQ(result.track_adapter_settings().target_width(), kIdealWidth);
  EXPECT_EQ(result.track_adapter_settings().target_height(), kIdealHeight);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       IdealResizeModeResolutionLessThanNative) {
  // Ideal resolution is slightly less than the closest native resolution.
  const int kIdealWidth = 639;
  const int kIdealHeight = 479;
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetIdeal(kIdealWidth);
  constraint_factory_.basic().height.SetIdeal(kIdealHeight);
  constraint_factory_.basic().resize_mode.SetIdeal(
      WebString::FromASCII("none"));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // A native mode of 640x480 should be selected since it is the closest native
  // mode to the ideal values.
  EXPECT_EQ(result.Width(), 640);
  EXPECT_EQ(result.Height(), 480);
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());

  constraint_factory_.basic().resize_mode.SetIdeal("crop-and-scale");
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Rescaling is preferred, therefore a native mode greater than the ideal
  // resolution is chosen.
  EXPECT_GE(result.Width(), kIdealWidth);
  EXPECT_GE(result.Height(), kIdealHeight);
  EXPECT_EQ(result.track_adapter_settings().target_width(), kIdealWidth);
  EXPECT_EQ(result.track_adapter_settings().target_height(), kIdealHeight);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealResizeFarFromNative) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetIdeal(1);
  constraint_factory_.basic().height.SetIdeal(1);
  constraint_factory_.basic().resize_mode.SetIdeal(
      WebString::FromASCII("none"));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The native mode closest to 1x1 is 40x30 with the low-res device.
  EXPECT_EQ(result.device_id(), low_res_device_->device_id.Utf8());
  EXPECT_EQ(result.Width(), 40);
  EXPECT_EQ(result.Height(), 30);
  // Despite resize_mode being ideal "none", SelectSettings opts for rescaling
  // since the fitness distance of 40x30 with respect to the ideal 1x1 is larger
  // than the fitness distance for resize_mode not being "none"
  // (29/30 + 39/40 > 1.0)
  EXPECT_TRUE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(result.track_adapter_settings().target_width(), 1);
  EXPECT_EQ(result.track_adapter_settings().target_height(), 1);

  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetIdeal(1);
  constraint_factory_.basic().resize_mode.SetIdeal(
      WebString::FromASCII("none"));
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The native mode closest to 1x1 is 40x30 with the low-res device.
  EXPECT_EQ(result.device_id(), low_res_device_->device_id.Utf8());
  EXPECT_EQ(result.Width(), 40);
  EXPECT_EQ(result.Height(), 30);
  // In this case, SelectSettings opts for not rescaling since the fitness
  // distance of width 40 with respect to the ideal 1 is larger than the
  // fitness distance for resize_mode not being "none" (39/40 < 1.0)
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, TwoIdealResizeValues) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetIdeal(641);
  constraint_factory_.basic().height.SetIdeal(481);
  constraint_factory_.basic().resize_mode.SetIdeal({"none", "crop-and-scale"});
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // 800x600 rescaled to 641x481 is closest to the specified ideal values.
  EXPECT_EQ(result.device_id(), low_res_device_->device_id.Utf8());
  EXPECT_EQ(result.Width(), 800);
  EXPECT_EQ(result.Height(), 600);
  // Since both resize modes are considered ideal, rescaling is preferred
  // because of the penalty due to deviating from the ideal reo
  EXPECT_TRUE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(result.track_adapter_settings().target_width(), 641);
  EXPECT_EQ(result.track_adapter_settings().target_height(), 481);

  constraint_factory_.Reset();
  constraint_factory_.basic().resize_mode.SetIdeal({"none", "crop-and-scale"});
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Given that both resize modes are ideal, the default device with the
  // resolution closest to the default without rescaling is selected.
  EXPECT_EQ(result.device_id(), default_device_->device_id.Utf8());
  EXPECT_EQ(result.Width(), 500);
  EXPECT_EQ(result.Height(), 500);
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryExactPanTiltZoom) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetExact(3);
    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    // The algorithm should prefer the first device that supports PTZ natively,
    // which is the low-res device.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    if (constraint == &MediaTrackConstraintSetPlatform::pan) {
      EXPECT_EQ(3, result.image_capture_device_settings()->pan.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::tilt) {
      EXPECT_EQ(3, result.image_capture_device_settings()->tilt.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::zoom) {
      EXPECT_EQ(3, result.image_capture_device_settings()->zoom.value());
    }
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMinPanTiltZoom) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetMin(2);
    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    // The algorithm should prefer the first device that supports PTZ
    // natively, which is the low-res device.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    if (constraint == &MediaTrackConstraintSetPlatform::pan) {
      EXPECT_EQ(2, result.image_capture_device_settings()->pan.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::tilt) {
      EXPECT_EQ(2, result.image_capture_device_settings()->tilt.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::zoom) {
      EXPECT_EQ(2, result.image_capture_device_settings()->zoom.value());
    }
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryMaxPanTiltZoom) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetMax(4);
    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    // The algorithm should prefer the first device that supports PTZ
    // natively, which is the low-res device.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    if (constraint == &MediaTrackConstraintSetPlatform::pan) {
      EXPECT_EQ(4, result.image_capture_device_settings()->pan.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::tilt) {
      EXPECT_EQ(4, result.image_capture_device_settings()->tilt.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::zoom) {
      EXPECT_EQ(4, result.image_capture_device_settings()->zoom.value());
    }
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, MandatoryPanTiltZoomRange) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetMin(2);
    (constraint_factory_.basic().*constraint).SetMax(4);
    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    // The algorithm should prefer the first device that supports PTZ
    // natively, which is the low-res device.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    if (constraint == &MediaTrackConstraintSetPlatform::pan) {
      EXPECT_EQ(2, result.image_capture_device_settings()->pan.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::tilt) {
      EXPECT_EQ(2, result.image_capture_device_settings()->tilt.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::zoom) {
      EXPECT_EQ(2, result.image_capture_device_settings()->zoom.value());
    }
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, IdealPanTiltZoom) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetIdeal(3);
    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    // The algorithm should select the first device that supports the ideal PTZ
    // constraint natively, which is the low-res device.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    if (constraint == &MediaTrackConstraintSetPlatform::pan) {
      EXPECT_EQ(3, result.image_capture_device_settings()->pan.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::tilt) {
      EXPECT_EQ(3, result.image_capture_device_settings()->tilt.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::zoom) {
      EXPECT_EQ(3, result.image_capture_device_settings()->zoom.value());
    }
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, PresentPanTiltZoom) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetIsPresent(true);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The algorithm should select the first device that supports the boolean
    // PTZ constraint natively, which is the low-res device.
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       PresentPanTiltZoomOnSystemWithoutPanTiltZoomCamera) {
  // Simulate a system with camera that does not support PTZ.
  // Manually adding device capabilities because VideoDeviceCaptureCapabilities
  // is move only.
  VideoDeviceCaptureCapabilities capabilities;
  VideoInputDeviceCapabilities device;
  device.device_id = kDeviceID1;
  device.facing_mode = mojom::blink::FacingMode::kNone;
  device.formats = {
      media::VideoCaptureFormat(gfx::Size(200, 200), 40.0f,
                                media::PIXEL_FORMAT_I420),
  };
  device.control_support.pan = false;
  device.control_support.tilt = false;
  device.control_support.zoom = false;
  capabilities.device_capabilities.push_back(std::move(device));
  capabilities.noise_reduction_capabilities = {
      std::optional<bool>(),
      std::optional<bool>(true),
      std::optional<bool>(false),
  };

  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetIsPresent(true);
    auto constraints = constraint_factory_.CreateMediaConstraints();
    auto result = SelectSettingsVideoDeviceCapture(capabilities, constraints);
    EXPECT_TRUE(result.HasValue());
    // The algorithm should select one device, even if it doesn't support PTZ.
    EXPECT_EQ(std::string(kDeviceID1), result.device_id());
  }
}

// The "Advanced" tests check selection criteria involving advanced constraint
// sets.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedMinMaxResolutionFrameRate) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.SetMin(4000);
  advanced1.height.SetMin(4000);
  // No device supports the first advanced set. This first advanced constraint
  // set is therefore ignored in all calls to SelectSettings().
  // Tie-breaker rule that applies is closeness to default settings.
  auto result = SelectSettings();
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*default_closest_format_, result.Format());
  CheckTrackAdapterSettingsEqualsFormat(result);

  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.SetMin(320);
  advanced2.height.SetMin(240);
  advanced2.width.SetMax(640);
  advanced2.height.SetMax(480);
  result = SelectSettings();
  // The device that best supports this advanced set is the low-res device,
  // which natively supports the maximum resolution.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(320.0 / 480.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(640.0 / 240.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);

  MediaTrackConstraintSetPlatform& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.frame_rate.SetMax(10.0);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The high-res device natively supports the third advanced set in addition
  // to the previous set and should be selected.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(320.0 / 480.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(640.0 / 240.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result, 10.0);

  MediaTrackConstraintSetPlatform& advanced4 =
      constraint_factory_.AddAdvanced();
  advanced4.width.SetMax(1000);
  advanced4.height.SetMax(1000);
  result = SelectSettings();
  // The fourth advanced set does not change the allowed range set by previous
  // sets, so the selection is the same as in the previous case.
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(320.0 / 480.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(640.0 / 240.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result, 10.0);

  constraint_factory_.basic().width.SetIdeal(100);
  constraint_factory_.basic().height.SetIdeal(100);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The allowed resolution range set by constraints is [320x240-640x480], but
  // since the ideal resolution is 100x100, the preferred resolution in the
  // allowed range is 320x240.
  // With regards to frame rate, the maximum allowed is 10Hz.
  // This means that the track should be configured as 320x240@10Hz.
  // The low-res device at 320x240@30Hz is selected over the high-res device
  // at 640x400@10Hz because the distance between 320x240@30Hz and 320x240@10Hz
  // is lower than the distance between 640x400@10Hz and 320x240@10Hz.
  // Both candidates support standard fitness distance equally, since both can
  // use adjusments to produce 320x240@10Hz.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(320, result.Width());
  EXPECT_EQ(240, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(320.0 / 240.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(320.0 / 240.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result, 10.0);

  constraint_factory_.basic().width.SetIdeal(2000);
  constraint_factory_.basic().height.SetIdeal(1500);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The high-res device at 640x480@10Hz is closer to the large ideal
  // resolution.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(320.0 / 480.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(640.0 / 240.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result, 10.0);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedResolutionAndFrameRate) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.SetExact(1920);
  advanced1.height.SetExact(1080);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.frame_rate.SetExact(60.0);
  MediaTrackConstraintSetPlatform& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.width.SetExact(2304);
  advanced3.height.SetExact(1536);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The high-res device is the only one that satisfies the first advanced
  // set. 2304x1536x10.0 satisfies sets 1 and 3, while 1920x1080x60.0
  // satisfies sets 1, and 2. The latter must be selected, regardless of
  // any other criteria.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(1920, result.Width());
  EXPECT_EQ(1080, result.Height());
  EXPECT_EQ(60.0, result.FrameRate());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(1920.0 / 1080.0,
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(1920.0 / 1080.0,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result, 60.0);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, AdvancedNoiseReduction) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.SetMin(640);
  advanced1.height.SetMin(480);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.SetMin(1920);
  advanced2.height.SetMin(1080);
  advanced2.goog_noise_reduction.SetExact(false);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_LE(1920, result.Width());
  EXPECT_LE(1080, result.Height());
  EXPECT_TRUE(result.noise_reduction() && !*result.noise_reduction());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(1920.0 / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(result.Width() / 1080.0,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryNoiseReduction) {
  {
    constraint_factory_.Reset();
    MediaTrackConstraintSetPlatform& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.width.SetMin(640);
    advanced1.height.SetMin(480);
    advanced1.goog_noise_reduction.SetExact(true);
    MediaTrackConstraintSetPlatform& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.width.SetMin(1920);
    advanced2.height.SetMin(1080);
    advanced2.goog_noise_reduction.SetExact(false);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The second advanced set cannot be satisfied because it contradicts the
    // first set. The default device supports the first set and should be
    // selected.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_LE(640, result.Width());
    EXPECT_LE(480, result.Height());
    EXPECT_TRUE(result.noise_reduction() && *result.noise_reduction());
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(640.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(result.Width() / 480.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }

  // Same test without noise reduction
  {
    constraint_factory_.Reset();
    MediaTrackConstraintSetPlatform& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.width.SetMin(640);
    advanced1.height.SetMin(480);
    MediaTrackConstraintSetPlatform& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.width.SetMin(1920);
    advanced2.height.SetMin(1080);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // Only the high-res device can satisfy the second advanced set.
    EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
    EXPECT_LE(1920, result.Width());
    EXPECT_LE(1080, result.Height());
    // Should select default noise reduction setting.
    EXPECT_TRUE(!result.noise_reduction());
    EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
    EXPECT_EQ(1920.0 / result.Height(),
              result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(result.Width() / 1080.0,
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryExactResolution) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.SetExact(640);
  advanced1.height.SetExact(480);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.SetExact(1920);
  advanced2.height.SetExact(1080);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. The low-res device is the one that best supports the requested
  // resolution.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(640, result.Width());
  EXPECT_EQ(480, result.Height());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(640.0 / 480.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryMaxMinResolutionFrameRate) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.SetMax(640);
  advanced1.height.SetMax(480);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.SetMin(1920);
  advanced2.height.SetMin(1080);
  advanced2.frame_rate.SetExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. The default device with the 200x200@40Hz format should be selected.
  // That format satisfies the first advanced set as well as any other, so the
  // tie breaker rule that applies is default device ID.
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(200, result.Width());
  EXPECT_EQ(200, result.Height());
  EXPECT_EQ(40, result.FrameRate());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(1.0 / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(result.Width(), result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryMinMaxResolutionFrameRate) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.SetMin(800);
  advanced1.height.SetMin(600);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.SetMax(640);
  advanced2.height.SetMax(480);
  advanced2.frame_rate.SetExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. The default device with the 1000x1000@20Hz format should be selected.
  // That format satisfies the first advanced set as well as any other, so the
  // tie breaker rule that applies is default device ID.
  EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(1000, result.Width());
  EXPECT_EQ(1000, result.Height());
  EXPECT_EQ(20, result.FrameRate());
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(800.0 / result.Height(),
            result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(result.Width() / 600.0,
            result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryExactAspectRatio) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.aspect_ratio.SetExact(2300.0);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.aspect_ratio.SetExact(3.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. Only the high-res device in the highest-resolution format supports the
  // requested aspect ratio.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*high_res_highest_format_, result.Format());
  // The track is cropped to support the exact aspect ratio.
  EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
  EXPECT_EQ(std::round(result.Height() / 2300.0),
            result.track_adapter_settings().target_height());
  EXPECT_EQ(2300.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(2300.0, result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryAspectRatioRange) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.aspect_ratio.SetMin(2300.0);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.aspect_ratio.SetMax(3.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set. Only the high-res device in the highest-resolution format supports the
  // requested aspect ratio.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(*high_res_highest_format_, result.Format());
  // The track is cropped to support the min aspect ratio.
  EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
  EXPECT_EQ(std::round(result.Height() / 2300.0),
            result.track_adapter_settings().target_height());
  EXPECT_EQ(2300.0, result.track_adapter_settings().min_aspect_ratio());
  EXPECT_EQ(result.Width(), result.track_adapter_settings().max_aspect_ratio());
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryExactFrameRate) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.frame_rate.SetExact(40.0);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.frame_rate.SetExact(45.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set.
  EXPECT_EQ(40.0, result.FrameRate());
  CheckTrackAdapterSettingsEqualsResolution(result);
  CheckTrackAdapterSettingsEqualsFrameRate(result, 40.0);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryFrameRateRange) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.frame_rate.SetMin(40.0);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.frame_rate.SetMax(35.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set.
  EXPECT_LE(40.0, result.FrameRate());
  CheckTrackAdapterSettingsEqualsResolution(result);
  CheckTrackAdapterSettingsEqualsFrameRate(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryWidthFrameRate) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.width.SetMax(1920);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.width.SetMin(2000);
  advanced2.frame_rate.SetExact(10.0);
  MediaTrackConstraintSetPlatform& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.frame_rate.SetExact(30.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The low-res device at 320x240@30Hz satisfies advanced sets 1 and 3.
  // The high-res device at 2304x1536@10.0f can satisfy sets 1 and 2, but not
  // both at the same time. Thus, low-res device must be preferred.
  EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(30.0, result.FrameRate());
  EXPECT_GE(1920, result.Width());
  CheckTrackAdapterSettingsEqualsResolution(result);
  CheckTrackAdapterSettingsEqualsFrameRate(result, 30.0);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryHeightFrameRate) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.height.SetMax(1080);
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.height.SetMin(1500);
  advanced2.frame_rate.SetExact(10.0);
  MediaTrackConstraintSetPlatform& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.frame_rate.SetExact(60.0);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The high-res device at 1280x768@60Hz and 1920x1080@60Hz satisfies advanced
  // sets 1 and 3. The same device at 2304x1536@10.0f can satisfy sets 1 and 2,
  // but not both at the same time. Thus, the format closest to default that
  // satisfies sets 1 and 3 must be chosen.
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(60.0, result.FrameRate());
  EXPECT_GE(1080, result.Height());
  CheckTrackAdapterSettingsEqualsResolution(result);
  CheckTrackAdapterSettingsEqualsFrameRate(result, 60.0);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, AdvancedDeviceID) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  Vector<String> id_vector1 = {kDeviceID1, kDeviceID2};
  advanced1.device_id.SetExact(id_vector1);
  Vector<String> id_vector2 = {kDeviceID2, kDeviceID3};
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.device_id.SetExact(id_vector2);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kDeviceID2 must be selected because it is the only one that satisfies both
  // advanced sets.
  EXPECT_EQ(std::string(kDeviceID2), result.device_id());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, AdvancedGroupID) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  Vector<String> id_vector1 = {kGroupID1, kGroupID2};
  advanced1.group_id.SetExact(id_vector1);
  Vector<String> id_vector2 = {kGroupID2, kGroupID3};
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.group_id.SetExact(id_vector2);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The device with group_id kGroupID2 must be selected because it is the only
  // one that satisfies both advanced sets.
  EXPECT_EQ(std::string(kDeviceID2), result.device_id());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryDeviceID) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  Vector<String> id_vector1 = {kDeviceID1, kDeviceID2};
  advanced1.device_id.SetExact(id_vector1);
  Vector<String> id_vector2 = {kDeviceID3, kDeviceID4};
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.device_id.SetExact(id_vector2);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set.
  EXPECT_EQ(std::string(kDeviceID1), result.device_id());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryDeviceIDAndResolution) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  advanced1.device_id.SetExact({low_res_device_->device_id});

  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.device_id.SetExact({high_res_device_->device_id});
  advanced2.width.SetMax(50);
  advanced2.height.SetMax(50);

  MediaTrackConstraintSetPlatform& advanced3 =
      constraint_factory_.AddAdvanced();
  advanced3.width.SetExact(800);
  advanced3.height.SetExact(600);

  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set, but the third set must be applied.
  EXPECT_EQ(result.device_id(), low_res_device_->device_id.Utf8());
  EXPECT_EQ(result.Width(), 800);
  EXPECT_EQ(result.Height(), 600);
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryGroupID) {
  constraint_factory_.Reset();
  MediaTrackConstraintSetPlatform& advanced1 =
      constraint_factory_.AddAdvanced();
  Vector<String> id_vector1 = {kGroupID1, kGroupID2};
  advanced1.group_id.SetExact(id_vector1);
  Vector<String> id_vector2 = {kGroupID3, kGroupID4};
  MediaTrackConstraintSetPlatform& advanced2 =
      constraint_factory_.AddAdvanced();
  advanced2.group_id.SetExact(id_vector2);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The second advanced set must be ignored because it contradicts the first
  // set.
  EXPECT_EQ(std::string(kDeviceID1), result.device_id());
  CheckTrackAdapterSettingsEqualsFormat(result);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryAspectRatioWidth) {
  {
    constraint_factory_.Reset();
    MediaTrackConstraintSetPlatform& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.aspect_ratio.SetMin(17);
    MediaTrackConstraintSetPlatform& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.width.SetMax(1);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    // The second advanced set cannot be satisfied because it contradicts the
    // second set. The default device supports the first set and should be
    // selected.
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    EXPECT_EQ(*default_closest_format_, result.Format());
    EXPECT_EQ(result.Width(), result.track_adapter_settings().target_width());
    EXPECT_EQ(std::round(result.Width() / 17.0),
              result.track_adapter_settings().target_height());
    EXPECT_EQ(17, result.track_adapter_settings().min_aspect_ratio());
    EXPECT_EQ(result.Width(),
              result.track_adapter_settings().max_aspect_ratio());
    CheckTrackAdapterSettingsEqualsFrameRate(result);
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryImageCapture) {
  for (auto& constraint : BooleanImageCaptureConstraints()) {
    constraint_factory_.Reset();

    MediaTrackConstraintSetPlatform& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.device_id.SetExact({low_res_device_->device_id});

    MediaTrackConstraintSetPlatform& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.device_id.SetExact({default_device_->device_id});
    (advanced2.*constraint).SetExact(true);

    MediaTrackConstraintSetPlatform& advanced3 =
        constraint_factory_.AddAdvanced();
    (advanced3.*constraint).SetExact(false);

    MediaTrackConstraintSetPlatform& advanced4 =
        constraint_factory_.AddAdvanced();
    (advanced4.*constraint).SetExact(true);

    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    // The second advanced set must be ignored because it contradicts the first
    // set. The third advanced set must be applied. The fourth advanced must be
    // ignored because it contradicts the third set.
    EXPECT_EQ(result.image_capture_device_settings()->torch.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::torch);
    if (result.image_capture_device_settings()->torch.has_value()) {
      EXPECT_FALSE(result.image_capture_device_settings()->torch.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()->background_blur.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::background_blur);
    if (result.image_capture_device_settings()->background_blur.has_value()) {
      EXPECT_FALSE(
          result.image_capture_device_settings()->background_blur.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()
            ->background_segmentation_mask.has_value(),
        constraint ==
            &MediaTrackConstraintSetPlatform::background_segmentation_mask);
    if (result.image_capture_device_settings()
            ->background_segmentation_mask.has_value()) {
      EXPECT_FALSE(result.image_capture_device_settings()
                       ->background_segmentation_mask.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()->eye_gaze_correction.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::eye_gaze_correction);
    if (result.image_capture_device_settings()
            ->eye_gaze_correction.has_value()) {
      EXPECT_FALSE(
          result.image_capture_device_settings()->eye_gaze_correction.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->face_framing.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::face_framing);
    if (result.image_capture_device_settings()->face_framing.has_value()) {
      EXPECT_FALSE(
          result.image_capture_device_settings()->face_framing.value());
    }
  }

  int value = 0;
  for (auto& constraint : DoubleImageCaptureConstraints()) {
    constraint_factory_.Reset();

    MediaTrackConstraintSetPlatform& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.device_id.SetExact({low_res_device_->device_id});

    MediaTrackConstraintSetPlatform& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.device_id.SetExact({default_device_->device_id});
    switch (++value % 4) {
      case 0:
        (advanced2.*constraint).SetExact(value + 1.0);
        break;
      case 1:
        (advanced2.*constraint).SetExact(value + 1.0);
        break;
      case 2:
        (advanced2.*constraint).SetExact(value - 1.0);
        break;
      case 3:
        (advanced2.*constraint).SetExact(value + 1.0);
        break;
    }

    MediaTrackConstraintSetPlatform& advanced3 =
        constraint_factory_.AddAdvanced();
    switch (value % 4) {
      case 0:
        (advanced3.*constraint).SetExact(value);
        break;
      case 1:
        (advanced3.*constraint).SetMin(value);
        break;
      case 2:
        (advanced3.*constraint).SetMax(value);
        break;
      case 3:
        (advanced3.*constraint).SetMin(value - 2.0);
        (advanced3.*constraint).SetMax(value + 2.0);
        break;
    }

    MediaTrackConstraintSetPlatform& advanced4 =
        constraint_factory_.AddAdvanced();
    switch (value % 4) {
      case 0:
        (advanced4.*constraint).SetExact(value - 1.0);
        break;
      case 1:
        (advanced4.*constraint).SetExact(value - 1.0);
        break;
      case 2:
        (advanced4.*constraint).SetExact(value + 1.0);
        break;
      case 3:
        (advanced4.*constraint).SetExact(value + 3.0);
        break;
    }

    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    // The second advanced set must be ignored because it contradicts the first
    // set. The third advanced set must be applied. The fourth advanced must be
    // ignored because it contradicts the third set.
    EXPECT_EQ(
        result.image_capture_device_settings()
            ->exposure_compensation.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::exposure_compensation);
    if (result.image_capture_device_settings()
            ->exposure_compensation.has_value()) {
      EXPECT_EQ(1.0, result.image_capture_device_settings()
                         ->exposure_compensation.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->exposure_time.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::exposure_time);
    if (result.image_capture_device_settings()->exposure_time.has_value()) {
      EXPECT_EQ(2.0,
                result.image_capture_device_settings()->exposure_time.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()->color_temperature.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::color_temperature);
    if (result.image_capture_device_settings()->color_temperature.has_value()) {
      EXPECT_EQ(
          3.0,
          result.image_capture_device_settings()->color_temperature.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->iso.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::iso);
    if (result.image_capture_device_settings()->iso.has_value()) {
      EXPECT_EQ(4.0, result.image_capture_device_settings()->iso.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->brightness.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::brightness);
    if (result.image_capture_device_settings()->brightness.has_value()) {
      EXPECT_EQ(5.0,
                result.image_capture_device_settings()->brightness.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->contrast.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::contrast);
    if (result.image_capture_device_settings()->contrast.has_value()) {
      EXPECT_EQ(6.0, result.image_capture_device_settings()->contrast.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->saturation.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::saturation);
    if (result.image_capture_device_settings()->saturation.has_value()) {
      EXPECT_EQ(7.0,
                result.image_capture_device_settings()->saturation.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->sharpness.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::sharpness);
    if (result.image_capture_device_settings()->sharpness.has_value()) {
      EXPECT_EQ(8.0, result.image_capture_device_settings()->sharpness.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()->focus_distance.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::focus_distance);
    if (result.image_capture_device_settings()->focus_distance.has_value()) {
      EXPECT_EQ(9.0,
                result.image_capture_device_settings()->focus_distance.value());
    }
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedContradictoryPanTiltZoom) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();

    MediaTrackConstraintSetPlatform& advanced1 =
        constraint_factory_.AddAdvanced();
    advanced1.device_id.SetExact({low_res_device_->device_id});

    MediaTrackConstraintSetPlatform& advanced2 =
        constraint_factory_.AddAdvanced();
    advanced2.device_id.SetExact({default_device_->device_id});
    (advanced2.*constraint).SetExact(4);

    MediaTrackConstraintSetPlatform& advanced3 =
        constraint_factory_.AddAdvanced();
    (advanced3.*constraint).SetMin(4);
    (advanced3.*constraint).SetMax(2);

    MediaTrackConstraintSetPlatform& advanced4 =
        constraint_factory_.AddAdvanced();
    (advanced4.*constraint).SetExact(3);

    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(low_res_device_->device_id.Utf8(), result.device_id());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    // The second advanced set must be ignored because it contradicts the first
    // set. The third advanced must be ignored because it is invalid. The fourth
    // advanced set must be applied.
    if (constraint == &MediaTrackConstraintSetPlatform::pan) {
      EXPECT_EQ(3, result.image_capture_device_settings()->pan.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::tilt) {
      EXPECT_EQ(3, result.image_capture_device_settings()->tilt.value());
    } else if (constraint == &MediaTrackConstraintSetPlatform::zoom) {
      EXPECT_EQ(3, result.image_capture_device_settings()->zoom.value());
    }
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, AdvancedResize) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetIdeal(1);
  constraint_factory_.basic().height.SetIdeal(1);
  MediaTrackConstraintSetPlatform& advanced = constraint_factory_.AddAdvanced();

  advanced.resize_mode.SetExact("none");
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // The native mode closest to 1x1 is 40x30 with the low-res device.
  EXPECT_EQ(result.device_id(), low_res_device_->device_id.Utf8());
  EXPECT_EQ(result.Width(), 40);
  EXPECT_EQ(result.Height(), 30);
  // No rescaling occurs due to the advanced constraint specifying resizeMode
  // equal to "none".
  EXPECT_FALSE(result.track_adapter_settings().target_size().has_value());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       AdvancedResolutionResizeFrameRate) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetExact(639);

  // This advanced set must be ignored because there are no native resolutions
  // with width equal to 639.
  MediaTrackConstraintSetPlatform& advanced = constraint_factory_.AddAdvanced();
  advanced.resize_mode.SetExact("none");
  advanced.frame_rate.SetExact(19.0);

  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // Rescaling is enabled to satisfy the required resolution.
  EXPECT_TRUE(result.track_adapter_settings().target_size().has_value());
  EXPECT_EQ(result.track_adapter_settings().target_width(), 639);
  // Height gets adjusted as well to maintain the aspect ratio.
  EXPECT_EQ(result.track_adapter_settings().target_height(), 479);
  // Using native frame rate because the advanced set is ignored.
  EXPECT_EQ(result.track_adapter_settings().max_frame_rate(), std::nullopt);

  // The low-res device at 640x480@30Hz is the
  EXPECT_EQ(result.device_id(), low_res_device_->device_id.Utf8());
  EXPECT_EQ(result.Width(), 640);
  EXPECT_EQ(result.Height(), 480);
  EXPECT_EQ(result.FrameRate(), 30.0);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, AdvancedPanTiltZoom) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    constraint_factory_.basic().device_id.SetExact(default_device_->device_id);
    MediaTrackConstraintSetPlatform& advanced =
        constraint_factory_.AddAdvanced();
    (advanced.*constraint).SetExact(3);
    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(default_device_->device_id.Utf8(), result.device_id());
    // The advanced set must be ignored because the device does not support PTZ.
    EXPECT_FALSE(result.image_capture_device_settings().has_value());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, BasicContradictoryWidth) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetMin(10);
  constraint_factory_.basic().width.SetMax(9);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().width.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       BasicContradictoryWidthAspectRatio) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetMax(1);
  constraint_factory_.basic().aspect_ratio.SetExact(100.0);
  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(constraint_factory_.basic().aspect_ratio.GetName(),
            result.failed_constraint_name());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, BasicImageCapture) {
  for (auto& constraint : BooleanImageCaptureConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetIdeal(false);

    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    EXPECT_EQ(result.image_capture_device_settings()->torch.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::torch);
    if (result.image_capture_device_settings()->torch.has_value()) {
      EXPECT_FALSE(result.image_capture_device_settings()->torch.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()->background_blur.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::background_blur);
    if (result.image_capture_device_settings()->background_blur.has_value()) {
      EXPECT_FALSE(
          result.image_capture_device_settings()->background_blur.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()
            ->background_segmentation_mask.has_value(),
        constraint ==
            &MediaTrackConstraintSetPlatform::background_segmentation_mask);
    if (result.image_capture_device_settings()
            ->background_segmentation_mask.has_value()) {
      EXPECT_FALSE(result.image_capture_device_settings()
                       ->background_segmentation_mask.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()->eye_gaze_correction.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::eye_gaze_correction);
    if (result.image_capture_device_settings()
            ->eye_gaze_correction.has_value()) {
      EXPECT_FALSE(
          result.image_capture_device_settings()->eye_gaze_correction.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->face_framing.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::face_framing);
    if (result.image_capture_device_settings()->face_framing.has_value()) {
      EXPECT_FALSE(
          result.image_capture_device_settings()->face_framing.value());
    }
  }

  int value = 0;
  for (auto& constraint : DoubleImageCaptureConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetIdeal(++value);

    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    ASSERT_TRUE(result.image_capture_device_settings().has_value());
    EXPECT_EQ(
        result.image_capture_device_settings()
            ->exposure_compensation.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::exposure_compensation);
    if (result.image_capture_device_settings()
            ->exposure_compensation.has_value()) {
      EXPECT_EQ(1.0, result.image_capture_device_settings()
                         ->exposure_compensation.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->exposure_time.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::exposure_time);
    if (result.image_capture_device_settings()->exposure_time.has_value()) {
      EXPECT_EQ(2.0,
                result.image_capture_device_settings()->exposure_time.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()->color_temperature.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::color_temperature);
    if (result.image_capture_device_settings()->color_temperature.has_value()) {
      EXPECT_EQ(
          3.0,
          result.image_capture_device_settings()->color_temperature.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->iso.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::iso);
    if (result.image_capture_device_settings()->iso.has_value()) {
      EXPECT_EQ(4.0, result.image_capture_device_settings()->iso.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->brightness.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::brightness);
    if (result.image_capture_device_settings()->brightness.has_value()) {
      EXPECT_EQ(5.0,
                result.image_capture_device_settings()->brightness.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->contrast.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::contrast);
    if (result.image_capture_device_settings()->contrast.has_value()) {
      EXPECT_EQ(6.0, result.image_capture_device_settings()->contrast.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->saturation.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::saturation);
    if (result.image_capture_device_settings()->saturation.has_value()) {
      EXPECT_EQ(7.0,
                result.image_capture_device_settings()->saturation.value());
    }
    EXPECT_EQ(result.image_capture_device_settings()->sharpness.has_value(),
              constraint == &MediaTrackConstraintSetPlatform::sharpness);
    if (result.image_capture_device_settings()->sharpness.has_value()) {
      EXPECT_EQ(8.0, result.image_capture_device_settings()->sharpness.value());
    }
    EXPECT_EQ(
        result.image_capture_device_settings()->focus_distance.has_value(),
        constraint == &MediaTrackConstraintSetPlatform::focus_distance);
    if (result.image_capture_device_settings()->focus_distance.has_value()) {
      EXPECT_EQ(9.0,
                result.image_capture_device_settings()->focus_distance.value());
    }
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       BasicContradictoryImageCapture) {
  for (auto& constraint : DoubleImageCaptureConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetMin(4);
    (constraint_factory_.basic().*constraint).SetMax(2);
    auto result = SelectSettings();
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ((constraint_factory_.basic().*constraint).GetName(),
              result.failed_constraint_name());
  }
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       BasicContradictoryPanTiltZoom) {
  for (auto& constraint : PanTiltZoomConstraints()) {
    constraint_factory_.Reset();
    (constraint_factory_.basic().*constraint).SetMin(4);
    (constraint_factory_.basic().*constraint).SetMax(2);
    auto result = SelectSettings();
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ((constraint_factory_.basic().*constraint).GetName(),
              result.failed_constraint_name());
  }
}

// The "NoDevices" tests verify that the algorithm returns the expected result
// when there are no candidates to choose from.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, NoDevicesNoConstraints) {
  constraint_factory_.Reset();
  VideoDeviceCaptureCapabilities capabilities;
  auto result = SelectSettingsVideoDeviceCapture(
      capabilities, constraint_factory_.CreateMediaConstraints());
  EXPECT_FALSE(result.HasValue());
  EXPECT_TRUE(std::string(result.failed_constraint_name()).empty());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, NoDevicesWithConstraints) {
  constraint_factory_.Reset();
  constraint_factory_.basic().height.SetExact(100);
  VideoDeviceCaptureCapabilities capabilities;
  auto result = SelectSettingsVideoDeviceCapture(
      capabilities, constraint_factory_.CreateMediaConstraints());
  EXPECT_FALSE(result.HasValue());
  EXPECT_TRUE(std::string(result.failed_constraint_name()).empty());
}

// This test verifies that having a device that reports a frame rate lower than
// 1 fps works.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, InvalidFrameRateDevice) {
  constraint_factory_.Reset();
  constraint_factory_.basic().device_id.SetExact(
      invalid_frame_rate_device_->device_id);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(invalid_frame_rate_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(invalid_frame_rate_device_->formats[0].frame_rate,
            result.FrameRate());
  EXPECT_EQ(result.FrameRate(), 0.0);
  EXPECT_FALSE(result.min_frame_rate().has_value());
  EXPECT_FALSE(result.max_frame_rate().has_value());

  // Select the second format with invalid frame rate.
  constraint_factory_.basic().width.SetExact(500);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(invalid_frame_rate_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(invalid_frame_rate_device_->formats[1].frame_rate,
            result.FrameRate());
  EXPECT_LT(result.FrameRate(), 1.0);
  EXPECT_FALSE(result.min_frame_rate().has_value());
  EXPECT_FALSE(result.max_frame_rate().has_value());
}

// This test verifies that an inverted default resolution is not preferred over
// the actual default resolution.
TEST_F(MediaStreamConstraintsUtilVideoDeviceTest, InvertedDefaultResolution) {
  constraint_factory_.Reset();
  constraint_factory_.basic().device_id.SetExact(high_res_device_->device_id);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  EXPECT_EQ(high_res_device_->device_id.Utf8(), result.device_id());
  EXPECT_EQ(result.Width(), MediaStreamVideoSource::kDefaultWidth);
  EXPECT_EQ(result.Height(), MediaStreamVideoSource::kDefaultHeight);
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       SelectEligibleSettingsVideoDeviceCapture_NoEligibleDevices) {
  constraint_factory_.Reset();
  constraint_factory_.basic().device_id.SetExact("NONEXISTING");
  auto result = SelectEligibleSettings();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(constraint_factory_.basic().device_id.GetName(), result.error());
}

TEST_F(MediaStreamConstraintsUtilVideoDeviceTest,
       SelectEligibleSettingsVideoDeviceCapture_IncludesEligibleDevices) {
  constraint_factory_.Reset();
  constraint_factory_.basic().width.SetMin(900);
  auto result = SelectEligibleSettings();
  EXPECT_TRUE(result.has_value());
  // Vector<VideoCaptureSettings> expected_settings;
  EXPECT_EQ(2u, result.value().size());
  EXPECT_EQ("fake_device_1", result.value()[0].device_id());
  EXPECT_EQ(gfx::Size(1000, 1000), result.value()[0].Format().frame_size);
  EXPECT_EQ("fake_device_3", result.value()[1].device_id());
  EXPECT_EQ(gfx::Size(1280, 720), result.value()[1].Format().frame_size);
}

}  // namespace blink
