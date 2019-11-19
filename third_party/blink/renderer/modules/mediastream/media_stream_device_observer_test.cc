// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_device_observer.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"

namespace blink {

class MediaStreamDeviceObserverTest : public ::testing::Test {
 public:
  MediaStreamDeviceObserverTest()
      : observer_(std::make_unique<MediaStreamDeviceObserver>(nullptr)) {}

  void OnDeviceOpened(base::OnceClosure quit_closure,
                      bool success,
                      const String& label,
                      const blink::MediaStreamDevice& device) {
    if (success) {
      stream_label_ = label;
      current_device_ = device;
      observer_->AddStream(label, device);
    }

    std::move(quit_closure).Run();
  }

 protected:
  String stream_label_;
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  std::unique_ptr<MediaStreamDeviceObserver> observer_;
  blink::MediaStreamDevice current_device_;
};

TEST_F(MediaStreamDeviceObserverTest, GetNonScreenCaptureDevices) {
  const int kRequestId1 = 5;
  const int kRequestId2 = 7;

  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  // OpenDevice request 1
  base::RunLoop run_loop1;
  mock_dispatcher_host_.OpenDevice(
      kRequestId1, "device_path",
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      base::BindOnce(&MediaStreamDeviceObserverTest::OnDeviceOpened,
                     base::Unretained(this), run_loop1.QuitClosure()));
  run_loop1.Run();
  String stream_label1 = stream_label_;

  // OpenDevice request 2
  base::RunLoop run_loop2;
  mock_dispatcher_host_.OpenDevice(
      kRequestId2, "screen_capture",
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      base::BindOnce(&MediaStreamDeviceObserverTest::OnDeviceOpened,
                     base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
  String stream_label2 = stream_label_;

  EXPECT_EQ(observer_->label_stream_map_.size(), 2u);

  // Only the device with type
  // blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE will be returned.
  blink::MediaStreamDevices video_devices =
      observer_->GetNonScreenCaptureDevices();
  EXPECT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].type,
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);

  // Close the device from request 2.
  observer_->RemoveStream(stream_label2);
  EXPECT_TRUE(observer_->GetVideoSessionId(stream_label2).is_empty());

  // Close the device from request 1.
  observer_->RemoveStream(stream_label1);
  EXPECT_TRUE(observer_->GetVideoSessionId(stream_label1).is_empty());

  // Verify that the request have been completed.
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);
}

TEST_F(MediaStreamDeviceObserverTest, OnDeviceStopped) {
  const int kRequestId = 5;

  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  // OpenDevice request.
  base::RunLoop run_loop1;
  mock_dispatcher_host_.OpenDevice(
      kRequestId, "device_path",
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      base::BindOnce(&MediaStreamDeviceObserverTest::OnDeviceOpened,
                     base::Unretained(this), run_loop1.QuitClosure()));
  run_loop1.Run();

  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);

  observer_->OnDeviceStopped(stream_label_, current_device_);

  // Verify that the request have been completed.
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);
}

TEST_F(MediaStreamDeviceObserverTest, OnDeviceChanged) {
  const int kRequestId1 = 5;
  const base::UnguessableToken kSessionId = base::UnguessableToken::Create();
  const String example_video_id1 = "fake_video_device1";
  const String example_video_id2 = "fake_video_device2";

  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  // OpenDevice request.
  base::RunLoop run_loop1;
  mock_dispatcher_host_.OpenDevice(
      kRequestId1, example_video_id1,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      base::BindOnce(&MediaStreamDeviceObserverTest::OnDeviceOpened,
                     base::Unretained(this), run_loop1.QuitClosure()));
  run_loop1.Run();

  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);
  blink::MediaStreamDevices video_devices =
      observer_->GetNonScreenCaptureDevices();
  EXPECT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].id, example_video_id1.Utf8());

  // OnDeviceChange request.
  blink::MediaStreamDevice fake_video_device(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      example_video_id2.Utf8(), "Fake Video Device");
  fake_video_device.set_session_id(kSessionId);
  observer_->OnDeviceChanged(stream_label_, current_device_, fake_video_device);

  // Verify that the device has been changed to the new |fake_video_device|.
  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);
  video_devices = observer_->GetNonScreenCaptureDevices();
  EXPECT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].id, example_video_id2.Utf8());
  EXPECT_EQ(video_devices[0].session_id(), kSessionId);

  // Close the device from request.
  observer_->RemoveStream(stream_label_);
  EXPECT_TRUE(observer_->GetVideoSessionId(stream_label_).is_empty());

  // Verify that the request have been completed.
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);
}

}  // namespace blink
