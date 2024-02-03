// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_devices_mojom_traits.h"

#include "media/capture/video/video_capture_device_descriptor.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-shared.h"

TEST(MediaDevicesMojomTraitsTest, Serialization) {
  const std::string device_id = "device_id";
  const std::string label = "label";
  const std::string group_id = "group_id";
  const media::VideoCaptureControlSupport video_control_support = {
      .pan = true, .tilt = true, .zoom = true};
  const blink::mojom::FacingMode video_facing =
      blink::mojom::FacingMode::kEnvironment;
  const media::CameraAvailability availability =
      media::CameraAvailability::kUnavailableExclusivelyUsedByOtherApplication;
  blink::WebMediaDeviceInfo input(device_id, label, group_id,
                                  video_control_support, video_facing,
                                  availability);
  blink::WebMediaDeviceInfo output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<blink::mojom::MediaDeviceInfo>(
          input, output));
  EXPECT_EQ(output.device_id, device_id);
  EXPECT_EQ(output.label, label);
  EXPECT_EQ(output.group_id, group_id);
  EXPECT_EQ(output.video_control_support.pan, video_control_support.pan);
  EXPECT_EQ(output.video_control_support.tilt, video_control_support.tilt);
  EXPECT_EQ(output.video_control_support.zoom, video_control_support.zoom);
  EXPECT_EQ(output.video_facing, video_facing);
  EXPECT_THAT(output.availability, testing::Optional(availability));
}
