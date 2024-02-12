// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/media_stream_devices_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(MediaStreamDevicesUtilTest, EmptyDevicesList) {
  auto filtered_devices = webrtc::FilterMediaDevices({}, {"id1", "id2"});
  blink::MediaStreamDevices expected_devices;
  EXPECT_EQ(expected_devices, filtered_devices);
}

TEST(MediaStreamDevicesUtilTest, NoOverlap) {
  blink::MediaStreamDevices devices = {{
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      "id1",
      "name 1",
  }};
  auto filtered_devices = webrtc::FilterMediaDevices(devices, {"id2", "id3"});
  blink::MediaStreamDevices expected_devices;
  EXPECT_EQ(expected_devices, filtered_devices);
}

TEST(MediaStreamDevicesUtilTest, OverlappingDevicesReturned) {
  blink::MediaStreamDevices devices = {
      {
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
          "id1",
          "name 1",
      },
      {
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
          "id2",
          "name 2",
      },
      {
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
          "id3",
          "name 3",
      }};
  auto filtered_devices = webrtc::FilterMediaDevices(devices, {"id3", "id2"});
  blink::MediaStreamDevices expected_devices = {devices[1], devices[2]};
  EXPECT_EQ(expected_devices, filtered_devices);
}
