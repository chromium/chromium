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
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class MediaStreamDeviceObserverTest : public ::testing::Test {
 public:
  MediaStreamDeviceObserverTest()
      : observer_(std::make_unique<MediaStreamDeviceObserver>(nullptr)) {}

  // Adds a stream holding a single video `device` under `label`, with no-op
  // callbacks.
  void AddVideoStream(const String& label, const MediaStreamDevice& device) {
    mojom::blink::StreamDevicesSet stream_devices_set;
    stream_devices_set.stream_devices.push_back(
        mojom::blink::StreamDevices::New(/*audio_device=*/std::nullopt,
                                         device));
    observer_->AddStreams(
        label, stream_devices_set,
        {
            .on_device_stopped_cb = base::DoNothing(),
            .on_device_changed_cb = base::DoNothing(),
            .on_device_request_state_change_cb = base::DoNothing(),
            .on_device_capture_configuration_change_cb = base::DoNothing(),
            .on_device_capture_handle_change_cb = base::DoNothing(),
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
            .on_zoom_level_change_cb = base::DoNothing(),
#endif
        });
  }

  void AddStreams(
      const String& streams_label,
      WebMediaStreamDeviceObserver::OnDeviceStoppedCb device_stopped_callback,
      WebMediaStreamDeviceObserver::OnDeviceRequestStateChangeCb
          request_state_change_callback) {
    wtf_size_t previous_stream_size = observer_->label_stream_map_.size();
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
      const Vector<MediaStreamDeviceObserver::Stream>& streams,
      const std::vector<std::string>& expected_labels) const {
    EXPECT_EQ(streams.size(), expected_labels.size());
    for (size_t stream_index = 0; stream_index < streams.size();
         ++stream_index) {
      EXPECT_EQ(
          streams[static_cast<wtf_size_t>(stream_index)].video_devices.size(),
          1u);
      EXPECT_EQ(
          streams[static_cast<wtf_size_t>(stream_index)].video_devices[0].id,
          expected_labels[stream_index]);
    }
  }

  const Vector<MediaStreamDeviceObserver::Stream>& GetStreams(
      const String& label) const {
    auto streams_iterator = observer_->label_stream_map_.find(label);
    EXPECT_NE(streams_iterator, observer_->label_stream_map_.end());
    return streams_iterator->value;
  }

  const MediaStreamDeviceObserver::Stream& GetStream(
      const String& label,
      wtf_size_t stream_index) const {
    return GetStreams(label)[stream_index];
  }

  void SetupMultiStreams(
      WebMediaStreamDeviceObserver::OnDeviceStoppedCb device_stopped_callback,
      WebMediaStreamDeviceObserver::OnDeviceRequestStateChangeCb
          request_state_change_callback) {
    const String streams_0_label = "label_0";
    AddStreams(streams_0_label, std::move(device_stopped_callback),
               std::move(request_state_change_callback));
    const Vector<MediaStreamDeviceObserver::Stream>& streams_0 =
        GetStreams(streams_0_label);
    CheckStreamDeviceIds(streams_0, {"device_0_id", "device_1_id"});
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<MediaStreamDeviceObserver> observer_;
};

TEST_F(MediaStreamDeviceObserverTest, GetNonScreenCaptureDevices) {
  const String kCameraStreamLabel = "camera_stream_label";
  const String kScreenStreamLabel = "screen_stream_label";
  MediaStreamDevice camera_device(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, "device_path",
      "Camera Device");
  MediaStreamDevice screen_device(
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      "screen_capture", "Screen Device");

  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  AddVideoStream(kCameraStreamLabel, camera_device);
  AddVideoStream(kScreenStreamLabel, screen_device);
  EXPECT_EQ(observer_->label_stream_map_.size(), 2u);

  // Only the device with type
  // blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE will be returned.
  blink::MediaStreamDevices video_devices =
      observer_->GetNonScreenCaptureDevices();
  EXPECT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].type,
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);

  // Close the screen capture device.
  observer_->RemoveStreamDevice(screen_device);
  EXPECT_FALSE(observer_->label_stream_map_.Contains(kScreenStreamLabel));
  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);

  // Close the camera device.
  observer_->RemoveStreamDevice(camera_device);
  EXPECT_FALSE(observer_->label_stream_map_.Contains(kCameraStreamLabel));

  // Verify that the request have been completed.
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);
}

TEST_F(MediaStreamDeviceObserverTest, OnDeviceStopped) {
  const String kStreamLabel = "stream_label";
  MediaStreamDevice device(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                           "device_path", "Camera Device");

  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  AddVideoStream(kStreamLabel, device);
  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);

  observer_->OnDeviceStopped(kStreamLabel, device);

  // Verify that the request have been completed.
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);
}

TEST_F(MediaStreamDeviceObserverTest, OnDeviceChanged) {
  const String kStreamLabel = "stream_label";
  const base::UnguessableToken kSessionId = base::UnguessableToken::Create();
  const String example_video_id1 = "fake_video_device1";
  const String example_video_id2 = "fake_video_device2";

  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  blink::MediaStreamDevice initial_video_device(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      example_video_id1.Utf8(), "Fake Video Device");
  AddVideoStream(kStreamLabel, initial_video_device);

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
  observer_->OnDeviceChanged(kStreamLabel, initial_video_device,
                             fake_video_device);

  // Verify that the device has been changed to the new |fake_video_device|.
  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);
  video_devices = observer_->GetNonScreenCaptureDevices();
  EXPECT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].id, example_video_id2.Utf8());
  EXPECT_EQ(video_devices[0].session_id(), kSessionId);

  // Close the changed device.
  observer_->RemoveStreamDevice(fake_video_device);

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
  AddVideoStream(kStreamLabel, initial_device);

  // Call the |observer_|'s bind callback and check that its internal
  // |receivers_| set is populated.
  mojo::Remote<mojom::blink::MediaStreamDeviceObserver> remote_observer1;
  EXPECT_TRUE(observer_->receivers_.empty());
  observer_->BindMediaStreamDeviceObserverReceiver(
      remote_observer1.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(observer_->receivers_.empty());

  // Send an OnDeviceChanged() message using the remote mojo pipe, and verify
  // that the device is changed.
  blink::MediaStreamDevice changed_device =
      blink::MediaStreamDevice(kDeviceType, "video_device-123", kDeviceName);
  remote_observer1->OnDeviceChanged(kStreamLabel, initial_device,
                                    changed_device);
  base::RunLoop().RunUntilIdle();
  blink::MediaStreamDevices video_devices =
      observer_->GetNonScreenCaptureDevices();
  ASSERT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].id, "video_device-123");

  // Bind a second remote (e.g., a second MediaStreamDispatcherHost in the same
  // frame) and verify that the first remote stays connected and both remotes
  // can deliver OnDeviceChanged() notifications concurrently.
  mojo::Remote<mojom::blink::MediaStreamDeviceObserver> remote_observer2;
  observer_->BindMediaStreamDeviceObserverReceiver(
      remote_observer2.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(remote_observer1.is_connected());
  EXPECT_TRUE(remote_observer2.is_connected());
  EXPECT_EQ(observer_->receivers_.size(), 2u);

  blink::MediaStreamDevice changed_device2 =
      blink::MediaStreamDevice(kDeviceType, "video_device-456", kDeviceName);
  remote_observer2->OnDeviceChanged(kStreamLabel, changed_device,
                                    changed_device2);
  base::RunLoop().RunUntilIdle();
  video_devices = observer_->GetNonScreenCaptureDevices();
  ASSERT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].id, "video_device-456");

  blink::MediaStreamDevice changed_device3 =
      blink::MediaStreamDevice(kDeviceType, "video_device-789", kDeviceName);
  remote_observer1->OnDeviceChanged(kStreamLabel, changed_device2,
                                    changed_device3);
  base::RunLoop().RunUntilIdle();
  video_devices = observer_->GetNonScreenCaptureDevices();
  ASSERT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].id, "video_device-789");

  // Resetting one remote leaves the other active; resetting and rebinding also
  // continues to deliver notifications.
  remote_observer1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer_->receivers_.size(), 1u);
  EXPECT_TRUE(remote_observer2.is_connected());

  remote_observer2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer_->receivers_.empty());

  observer_->BindMediaStreamDeviceObserverReceiver(
      remote_observer1.BindNewPipeAndPassReceiver());
  remote_observer1->OnDeviceChanged(
      kStreamLabel, changed_device3,
      blink::MediaStreamDevice(kDeviceType, "video_device-000", kDeviceName));
  base::RunLoop().RunUntilIdle();
  video_devices = observer_->GetNonScreenCaptureDevices();
  ASSERT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].id, "video_device-000");
}

TEST_F(MediaStreamDeviceObserverTest, OnDeviceRequestStateChange) {
  const String kStreamLabel = "stream_label";
  MediaStreamDevice device(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                           "device_path", "Camera Device");

  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  AddVideoStream(kStreamLabel, device);
  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);

  observer_->OnDeviceRequestStateChange(
      kStreamLabel, device, mojom::blink::MediaStreamStateChange::PAUSE);

  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);

  observer_->OnDeviceRequestStateChange(
      kStreamLabel, device, mojom::blink::MediaStreamStateChange::PLAY);

  EXPECT_EQ(observer_->label_stream_map_.size(), 1u);
}

TEST_F(MediaStreamDeviceObserverTest, MultiCaptureAddAndRemoveStreams) {
  EXPECT_EQ(observer_->label_stream_map_.size(), 0u);

  const String streams_label = "label_0";
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
  const Vector<MediaStreamDeviceObserver::Stream>& streams =
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

  const String streams_label = "label_0";
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

  const String streams_1_label = "label_1";
  AddStreams(streams_1_label, /*device_stopped_callback=*/base::DoNothing(),
             /*request_state_change_callback=*/base::DoNothing());
  const Vector<MediaStreamDeviceObserver::Stream>& streams_1 =
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
                EXPECT_EQ(
                    zoom_level,
                    CaptureController::getSupportedZoomLevelsForTabs()[0]);
              }),
      });
  static_cast<mojom::blink::MediaStreamDeviceObserver*>(observer_.get())
      ->OnZoomLevelChange(
          kStreamLabel, device,
          CaptureController::getSupportedZoomLevelsForTabs()[0]);
}
#endif

}  // namespace blink
