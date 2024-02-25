// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_device_observer.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"
#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

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

  void AddStreams(
      const WTF::String& streams_label,
      WebMediaStreamDeviceObserver::OnDeviceStoppedCb device_stopped_callback,
      WebMediaStreamDeviceObserver::OnDeviceRequestStateChangeCb
          request_state_change_callback) {
    WTF::wtf_size_t previous_stream_size = observer_->label_stream_map_.size();
    blink::mojom::blink::StreamDevicesSet stream_devices_set;
    stream_devices_set.stream_devices.push_back(
        blink::mojom::blink::StreamDevices::New(
            std::nullopt,
            MediaStreamDevice(
                blink::mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
                "device_0_id", "device_0_name")));
    stream_devices_set.stream_devices.push_back(
        blink::mojom::blink::StreamDevices::New(
            std::nullopt,
            MediaStreamDevice(
                blink::mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
                "device_1_id", "device_1_name")));

    observer_->AddStreams(
        streams_label, stream_devices_set,
        {
            .on_device_stopped_cb = device_stopped_callback,
            .on_device_changed_cb = base::DoNothing(),
            .on_device_request_state_change_cb = request_state_change_callback,
            .on_device_capture_configuration_change_cb = base::DoNothing(),
            .on_device_capture_handle_change_cb = base::DoNothing(),
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
            .on_zoom_level_change_cb = base::DoNothing(),
#endif
        });
    EXPECT_EQ(observer_->label_stream_map_.size(), previous_stream_size + 1);
  }

  void CheckStreamDeviceIds(
      const WTF::Vector<MediaStreamDeviceObserver::Stream>& streams,
      const std::vector<std::string>& expected_labels) const {
    EXPECT_EQ(streams.size(), expected_labels.size());
    for (size_t stream_index = 0; stream_index < streams.size();
         ++stream_index) {
      EXPECT_EQ(streams[static_cast<WTF::wtf_size_t>(stream_index)]
                    .video_devices.size(),
                1u);
      EXPECT_EQ(streams[static_cast<WTF::wtf_size_t>(stream_index)]
                    .video_devices[0]
                    .id,
                expected_labels[stream_index]);
    }
  }

  const WTF::Vector<MediaStreamDeviceObserver::Stream>& GetStreams(
      const WTF::String& label) const {
    auto streams_iterator = observer_->label_stream_map_.find(label);
    EXPECT_NE(streams_iterator, observer_->label_stream_map_.end());
    return streams_iterator->value;
  }

  const MediaStreamDeviceObserver::Stream& GetStream(
      const WTF::String& label,
      WTF::wtf_size_t stream_index) const {
    return GetStreams(label)[stream_index];
  }

  void SetupMultiStreams(
      WebMediaStreamDeviceObserver::OnDeviceStoppedCb device_stopped_callback,
      WebMediaStreamDeviceObserver::OnDeviceRequestStateChangeCb
          request_state_change_callback) {
    const WTF::String streams_0_label = "label_0";
    AddStreams(streams_0_label, std::move(device_stopped_callback),
               std::move(request_state_change_callback));
    const WTF::Vector<MediaStreamDeviceObserver::Stream>& streams_0 =
        GetStreams(streams_0_label);
    CheckStreamDeviceIds(streams_0, {"device_0_id", "device_1_id"});
  }

 protected:
  test::TaskEnvironment task_environment_;
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
  observer_->RemoveStreams(stream_label2);
  EXPECT_TRUE(observer_->GetVideoSessionId(stream_label2).is_empty());

  // Close the device from request 1.
  observer_->RemoveStreams(stream_label1);
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
  observer_->RemoveStreams(stream_label_);
  EXPECT_TRUE(observer_->GetVideoSessionId(stream_label_).is_empty());

  // Verify that the request have been completed.
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);
}

TEST_F(MediaStreamDeviceObserverTest, OnDeviceChangedChangesDeviceAfterRebind) {
  const String kStreamLabel = "stream_label";
  const std::string kDeviceName = "Video Device";
  const blink::mojom::MediaStreamType kDeviceType =
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;

  // Add a device to the |observer_|, to be changed using OnChangedDevice().
  blink::MediaStreamDevice initial_device(kDeviceType, "initial_device",
                                          kDeviceName);
  observer_->AddStream(kStreamLabel, initial_device);

  // Call the |observer_|'s bind callback and check that its internal
  // |receiver_| is bound.
  mojo::Remote<mojom::blink::MediaStreamDeviceObserver> remote_observer;
  EXPECT_FALSE(observer_->receiver_.is_bound());
  observer_->BindMediaStreamDeviceObserverReceiver(
      remote_observer.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(observer_->receiver_.is_bound());

  // Send an OnDeviceChanged() message using the remote mojo pipe, and verify
  // that the device is changed.
  blink::MediaStreamDevice changed_device =
      blink::MediaStreamDevice(kDeviceType, "video_device-123", kDeviceName);
  remote_observer->OnDeviceChanged(kStreamLabel, initial_device,
                                   changed_device);
  base::RunLoop().RunUntilIdle();
  blink::MediaStreamDevices video_devices =
      observer_->GetNonScreenCaptureDevices();
  ASSERT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].id, "video_device-123");

  // Reset the remote end of the mojo pipe, then rebind it, and verify that
  // OnDeviceChanged() changes the device after rebind.
  remote_observer.reset();
  observer_->BindMediaStreamDeviceObserverReceiver(
      remote_observer.BindNewPipeAndPassReceiver());
  remote_observer->OnDeviceChanged(
      kStreamLabel, changed_device,
      blink::MediaStreamDevice(kDeviceType, "video_device-456", kDeviceName));
  base::RunLoop().RunUntilIdle();
  video_devices = observer_->GetNonScreenCaptureDevices();
  ASSERT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].id, "video_device-456");
}

TEST_F(MediaStreamDeviceObserverTest, OnDeviceRequestStateChange) {
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

  observer_->OnDeviceRequestStateChange(
      stream_label_, current_device_,
      mojom::blink::MediaStreamStateChange::PAUSE);

  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);

  observer_->OnDeviceRequestStateChange(
      stream_label_, current_device_,
      mojom::blink::MediaStreamStateChange::PLAY);

  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);
}

TEST_F(MediaStreamDeviceObserverTest, MultiCaptureAddAndRemoveStreams) {
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  const WTF::String streams_label = "label_0";
  std::string latest_stopped_device_id;
  SetupMultiStreams(
      /*device_stopped_callback=*/base::BindLambdaForTesting(
          [&latest_stopped_device_id](const MediaStreamDevice& device) {
            latest_stopped_device_id = device.id;
          }),
      /*request_state_change_callback=*/base::DoNothing());

  MediaStreamDevice stopped_device_0(
      blink::mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
      "device_0_id", "device_0_name");
  observer_->OnDeviceStopped(streams_label, stopped_device_0);
  const WTF::Vector<MediaStreamDeviceObserver::Stream>& streams =
      GetStreams(streams_label);
  CheckStreamDeviceIds(streams, {"device_1_id"});
  EXPECT_EQ(latest_stopped_device_id, "device_0_id");

  MediaStreamDevice stopped_device_1(
      blink::mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
      "device_1_id", "device_1_name");
  observer_->OnDeviceStopped(streams_label, stopped_device_1);
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);
  EXPECT_EQ(latest_stopped_device_id, "device_1_id");
}

TEST_F(MediaStreamDeviceObserverTest, MultiCaptureChangeDeviceRequestState) {
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  const WTF::String streams_label = "label_0";
  std::string latest_changed_device_id;
  blink::mojom::blink::MediaStreamStateChange latest_device_state =
      blink::mojom::blink::MediaStreamStateChange::PAUSE;
  SetupMultiStreams(
      /*device_stopped_callback=*/base::DoNothing(),
      /*request_state_change_callback=*/base::BindLambdaForTesting(
          [&latest_changed_device_id, &latest_device_state](
              const MediaStreamDevice& device,
              const mojom::MediaStreamStateChange new_state) {
            latest_changed_device_id = device.id;
            latest_device_state = new_state;
          }));
  EXPECT_EQ(latest_changed_device_id, "");

  observer_->OnDeviceRequestStateChange(
      streams_label, GetStream(streams_label, 0).video_devices[0],
      blink::mojom::blink::MediaStreamStateChange::PLAY);
  EXPECT_EQ(latest_changed_device_id, "device_0_id");
  EXPECT_EQ(latest_device_state,
            blink::mojom::blink::MediaStreamStateChange::PLAY);

  observer_->OnDeviceRequestStateChange(
      streams_label, GetStream(streams_label, 1).video_devices[0],
      blink::mojom::blink::MediaStreamStateChange::PAUSE);
  EXPECT_EQ(latest_changed_device_id, "device_1_id");
  EXPECT_EQ(latest_device_state,
            blink::mojom::blink::MediaStreamStateChange::PAUSE);
}

TEST_F(MediaStreamDeviceObserverTest, MultiCaptureRemoveStreamDevice) {
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  SetupMultiStreams(/*device_stopped_callback=*/base::DoNothing(),
                    /*request_state_change_callback=*/base::DoNothing());

  const WTF::String streams_1_label = "label_1";
  AddStreams(streams_1_label, /*device_stopped_callback=*/base::DoNothing(),
             /*request_state_change_callback=*/base::DoNothing());
  const WTF::Vector<MediaStreamDeviceObserver::Stream>& streams_1 =
      GetStreams(streams_1_label);
  CheckStreamDeviceIds(streams_1, {"device_0_id", "device_1_id"});

  MediaStreamDevice device_0 = streams_1[0].video_devices[0];
  MediaStreamDevice device_1 = streams_1[1].video_devices[0];
  observer_->RemoveStreamDevice(device_0);
  EXPECT_EQ(streams_1.size(), 1u);
  EXPECT_EQ(streams_1[0].video_devices.size(), 1u);
  EXPECT_EQ(streams_1[0].video_devices[0].id, "device_1_id");

  observer_->RemoveStreamDevice(device_1);
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(MediaStreamDeviceObserverTest, OnZoomLevelChange) {
  const String kStreamLabel = "stream_label";
  blink::MediaStreamDevice device(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "device_id",
      "device_name");

  blink::mojom::blink::StreamDevicesSet stream_devices_set;
  stream_devices_set.stream_devices.push_back(
      blink::mojom::blink::StreamDevices::New(std::nullopt, device));
  observer_->AddStreams(
      kStreamLabel, stream_devices_set,
      {
          .on_device_stopped_cb = base::DoNothing(),
          .on_device_changed_cb = base::DoNothing(),
          .on_device_request_state_change_cb = base::DoNothing(),
          .on_device_capture_configuration_change_cb = base::DoNothing(),
          .on_device_capture_handle_change_cb = base::DoNothing(),
          .on_zoom_level_change_cb = base::BindRepeating(
              [](const MediaStreamDevice& device, int zoom_level) {
                EXPECT_EQ(device.type,
                          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
                EXPECT_EQ(device.id, "device_id");
                EXPECT_EQ(device.name, "device_name");
                EXPECT_EQ(zoom_level,
                          CaptureController::getSupportedZoomLevels()[0]);
              }),
      });
  static_cast<mojom::blink::MediaStreamDeviceObserver*>(observer_.get())
      ->OnZoomLevelChange(kStreamLabel, device,
                          CaptureController::getSupportedZoomLevels()[0]);
}
#endif

}  // namespace blink
