// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_dispatcher_host.h"

#include <stddef.h>

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using blink::mojom::MediaDeviceType;
using testing::_;
using testing::SaveArg;
using testing::InvokeWithoutArgs;

namespace content {

namespace {

const int kProcessId = 5;
const int kRenderId = 6;
const size_t kNumFakeVideoDevices = 3;
const char kNormalVideoDeviceID[] = "/dev/video0";
const char kNoFormatsVideoDeviceID[] = "/dev/video1";
const char kZeroResolutionVideoDeviceID[] = "/dev/video2";
const char* const kDefaultVideoDeviceID = kZeroResolutionVideoDeviceID;
const char kDefaultAudioDeviceID[] = "fake_audio_input_2";

const auto kIgnoreLogMessageCB = base::BindRepeating([](const std::string&) {});

void PhysicalDevicesEnumerated(base::Closure quit_closure,
                               MediaDeviceEnumeration* out,
                               const MediaDeviceEnumeration& enumeration) {
  *out = enumeration;
  std::move(quit_closure).Run();
}

class MockMediaDevicesListener : public blink::mojom::MediaDevicesListener {
 public:
  MockMediaDevicesListener() {}

  MOCK_METHOD2(OnDevicesChanged,
               void(blink::MediaDeviceType,
                    const blink::WebMediaDeviceInfoArray&));

  mojo::PendingRemote<blink::mojom::MediaDevicesListener>
  CreatePendingRemoteAndBind() {
    mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener;
    receivers_.Add(this, listener.InitWithNewPipeAndPassReceiver());
    return listener;
  }

 private:
  mojo::ReceiverSet<blink::mojom::MediaDevicesListener> receivers_;
};

}  // namespace

class MediaDevicesDispatcherHostTest : public testing::TestWithParam<GURL> {
 public:
  MediaDevicesDispatcherHostTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        origin_(url::Origin::Create(GetParam())) {
    // Make sure we use fake devices to avoid long delays.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("video-input-default-id=%s, "
                           "audio-input-default-id=%s",
                           kDefaultVideoDeviceID, kDefaultAudioDeviceID));
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());

    auto video_capture_device_factory =
        std::make_unique<media::FakeVideoCaptureDeviceFactory>();
    video_capture_device_factory_ = video_capture_device_factory.get();
    auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
        std::move(video_capture_device_factory));
    auto video_capture_provider =
        std::make_unique<InProcessVideoCaptureProvider>(
            std::move(video_capture_system),
            base::ThreadTaskRunnerHandle::Get(), kIgnoreLogMessageCB);

    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), audio_manager_->GetTaskRunner(),
        std::move(video_capture_provider));
    host_ = std::make_unique<MediaDevicesDispatcherHost>(
        kProcessId, kRenderId, media_stream_manager_.get());
    media_stream_manager_->media_devices_manager()
        ->set_salt_and_origin_callback_for_testing(base::BindRepeating(
            &MediaDevicesDispatcherHostTest::GetSaltAndOrigin,
            base::Unretained(this)));
  }
  ~MediaDevicesDispatcherHostTest() override { audio_manager_->Shutdown(); }

  void SetUp() override {
    std::vector<media::FakeVideoCaptureDeviceSettings> fake_video_devices(
        kNumFakeVideoDevices);
    // A regular video device
    fake_video_devices[0].device_id = kNormalVideoDeviceID;
    fake_video_devices[0].supported_formats = {
        {gfx::Size(640, 480), 30.0, media::PIXEL_FORMAT_I420},
        {gfx::Size(800, 600), 30.0, media::PIXEL_FORMAT_I420},
        {gfx::Size(1020, 780), 30.0, media::PIXEL_FORMAT_I420},
        {gfx::Size(1920, 1080), 20.0, media::PIXEL_FORMAT_I420},
    };
    // A video device that does not report any formats
    fake_video_devices[1].device_id = kNoFormatsVideoDeviceID;
    ASSERT_TRUE(fake_video_devices[1].supported_formats.empty());
    // A video device that reports a 0x0 resolution.
    fake_video_devices[2].device_id = kZeroResolutionVideoDeviceID;
    fake_video_devices[2].supported_formats = {
        {gfx::Size(0, 0), 0.0, media::PIXEL_FORMAT_I420},
    };
    video_capture_device_factory_->SetToCustomDevicesConfig(fake_video_devices);

    base::RunLoop run_loop;
    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
    devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] = true;
    devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = true;
    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&PhysicalDevicesEnumerated, run_loop.QuitClosure(),
                       &physical_devices_));
    run_loop.Run();

    ASSERT_GT(physical_devices_[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT].size(),
              0u);
    ASSERT_GT(physical_devices_[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT].size(),
              0u);
    ASSERT_GT(physical_devices_[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].size(),
              0u);
  }

  MOCK_METHOD1(
      UniqueOriginCallback,
      void(const std::vector<std::vector<blink::WebMediaDeviceInfo>>&));
  MOCK_METHOD1(
      ValidOriginCallback,
      void(const std::vector<std::vector<blink::WebMediaDeviceInfo>>&));
  MOCK_METHOD0(MockVideoInputCapabilitiesCallback, void());
  MOCK_METHOD0(MockAudioInputCapabilitiesCallback, void());
  MOCK_METHOD0(MockAllVideoInputDeviceFormatsCallback, void());
  MOCK_METHOD0(MockAvailableVideoInputDeviceFormatsCallback, void());

  void VideoInputCapabilitiesCallback(
      std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr> capabilities) {
    MockVideoInputCapabilitiesCallback();
    MediaDeviceSaltAndOrigin salt_and_origin =
        GetMediaDeviceSaltAndOrigin(-1, -1);
    std::string expected_first_device_id =
        GetHMACForMediaDeviceID(salt_and_origin.device_id_salt,
                                salt_and_origin.origin, kDefaultVideoDeviceID);
    EXPECT_EQ(kNumFakeVideoDevices, capabilities.size());
    EXPECT_EQ(expected_first_device_id, capabilities[0]->device_id);
    for (const auto& capability : capabilities) {
      // Always expect at least one format
      EXPECT_GT(capability->formats.size(), 1u);
      for (auto& format : capability->formats) {
        EXPECT_GE(format.frame_size.width(), 1);
        EXPECT_GE(format.frame_size.height(), 1);
        EXPECT_GE(format.frame_rate, 0.0);
      }
    }
  }

  void VideoInputCapabilitiesUniqueOriginCallback(
      std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr> capabilities) {
    MockVideoInputCapabilitiesCallback();
    EXPECT_EQ(0U, capabilities.size());
  }

  void AudioInputCapabilitiesCallback(
      std::vector<blink::mojom::AudioInputDeviceCapabilitiesPtr> capabilities) {
    MockAudioInputCapabilitiesCallback();
    // MediaDevicesManager always returns 3 fake audio input devices.
    const size_t kNumExpectedEntries = 3;
    EXPECT_EQ(kNumExpectedEntries, capabilities.size());
    MediaDeviceSaltAndOrigin salt_and_origin =
        GetMediaDeviceSaltAndOrigin(-1, -1);
    std::string expected_first_device_id =
        GetHMACForMediaDeviceID(salt_and_origin.device_id_salt,
                                salt_and_origin.origin, kDefaultAudioDeviceID);
    EXPECT_EQ(expected_first_device_id, capabilities[0]->device_id);
    for (const auto& capability : capabilities)
      EXPECT_TRUE(capability->parameters.IsValid());
  }

  void AllVideoInputDeviceFormatsCallback(
      const std::vector<media::VideoCaptureFormat>& formats) {
    MockAllVideoInputDeviceFormatsCallback();
    EXPECT_GT(formats.size(), 0U);
    for (const auto& format : formats) {
      EXPECT_GT(format.frame_size.GetArea(), 0);
      EXPECT_GE(format.frame_rate, 0.0);
    }
  }

  void AvailableVideoInputDeviceFormatsCallback(
      const std::vector<media::VideoCaptureFormat>& formats) {
    MockAvailableVideoInputDeviceFormatsCallback();
    EXPECT_EQ(formats.size(), 4U);
    EXPECT_EQ(formats[0], media::VideoCaptureFormat(gfx::Size(640, 480), 30.0,
                                                    media::PIXEL_FORMAT_I420));
    EXPECT_EQ(formats[1], media::VideoCaptureFormat(gfx::Size(800, 600), 30.0,
                                                    media::PIXEL_FORMAT_I420));
    EXPECT_EQ(formats[2], media::VideoCaptureFormat(gfx::Size(1020, 780), 30.0,
                                                    media::PIXEL_FORMAT_I420));
    EXPECT_EQ(formats[3], media::VideoCaptureFormat(gfx::Size(1920, 1080), 20.0,
                                                    media::PIXEL_FORMAT_I420));
  }

 protected:
  void DevicesEnumerated(
      const base::Closure& closure,
      const std::vector<std::vector<blink::WebMediaDeviceInfo>>& devices,
      std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr>
          video_input_capabilities,
      std::vector<blink::mojom::AudioInputDeviceCapabilitiesPtr>
          audio_input_capabilities) {
    enumerated_devices_ = devices;
    closure.Run();
  }

  void EnumerateDevicesAndWaitForResult(bool enumerate_audio_input,
                                        bool enumerate_video_input,
                                        bool enumerate_audio_output,
                                        bool permission_override_value = true) {
    media_stream_manager_->media_devices_manager()->SetPermissionChecker(
        std::make_unique<MediaDevicesPermissionChecker>(
            permission_override_value));
    base::RunLoop run_loop;
    host_->EnumerateDevices(
        enumerate_audio_input, enumerate_video_input, enumerate_audio_output,
        false, false,
        base::BindOnce(&MediaDevicesDispatcherHostTest::DevicesEnumerated,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    ASSERT_FALSE(enumerated_devices_.empty());
    if (enumerate_audio_input)
      EXPECT_FALSE(
          enumerated_devices_[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT].empty());
    if (enumerate_video_input)
      EXPECT_FALSE(
          enumerated_devices_[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT].empty());
    if (enumerate_audio_output)
      EXPECT_FALSE(
          enumerated_devices_[blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].empty());

    EXPECT_FALSE(DoesContainRawIds(enumerated_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(enumerated_devices_, origin_));
  }

  bool DoesContainRawIds(
      const std::vector<std::vector<blink::WebMediaDeviceInfo>>& enumeration) {
    for (size_t i = 0; i < blink::NUM_MEDIA_DEVICE_TYPES; ++i) {
      for (const auto& device_info : enumeration[i]) {
        for (const auto& raw_device_info : physical_devices_[i]) {
          // Skip default and communications audio devices, whose IDs are not
          // translated.
          if (device_info.device_id ==
                  media::AudioDeviceDescription::kDefaultDeviceId ||
              device_info.device_id ==
                  media::AudioDeviceDescription::kCommunicationsDeviceId) {
            continue;
          }
          if (device_info.device_id == raw_device_info.device_id)
            return true;
        }
      }
    }
    return false;
  }

  bool DoesEveryDeviceMapToRawId(
      const std::vector<std::vector<blink::WebMediaDeviceInfo>>& enumeration,
      const url::Origin& origin) {
    MediaDeviceSaltAndOrigin salt_and_origin =
        GetMediaDeviceSaltAndOrigin(-1, -1);
    for (size_t i = 0; i < blink::NUM_MEDIA_DEVICE_TYPES; ++i) {
      for (const auto& device_info : enumeration[i]) {
        bool found_match = false;
        for (const auto& raw_device_info : physical_devices_[i]) {
          if (DoesMediaDeviceIDMatchHMAC(
                  salt_and_origin.device_id_salt, salt_and_origin.origin,
                  device_info.device_id, raw_device_info.device_id)) {
            EXPECT_FALSE(found_match);
            found_match = true;
          }
        }
        if (!found_match)
          return false;
      }
    }
    return true;
  }

  // Returns true if all devices have labels, false otherwise.
  bool DoesContainLabels(const blink::WebMediaDeviceInfoArray& device_infos) {
    for (const auto& device_info : device_infos) {
      if (device_info.label.empty())
        return false;
    }
    return true;
  }

  // Returns true if all devices have labels, false otherwise.
  bool DoesContainLabels(
      const std::vector<blink::WebMediaDeviceInfoArray>& enumeration) {
    for (const auto& device_infos : enumeration) {
      if (!DoesContainLabels(device_infos))
        return false;
    }
    return true;
  }

  // Returns true if no devices have labels, false otherwise.
  bool DoesNotContainLabels(
      const blink::WebMediaDeviceInfoArray& device_infos) {
    for (const auto& device_info : device_infos) {
      if (!device_info.label.empty())
        return false;
    }
    return true;
  }

  // Returns true if no devices have labels, false otherwise.
  bool DoesNotContainLabels(
      const std::vector<std::vector<blink::WebMediaDeviceInfo>>& enumeration) {
    for (const auto& device_infos : enumeration) {
      if (!DoesNotContainLabels(device_infos))
        return false;
    }
    return true;
  }

  void SubscribeAndWaitForResult(bool has_permission) {
    media_stream_manager_->media_devices_manager()->SetPermissionChecker(
        std::make_unique<MediaDevicesPermissionChecker>(has_permission));
    MockMediaDevicesListener device_change_listener;
    for (size_t i = 0; i < blink::NUM_MEDIA_DEVICE_TYPES; ++i) {
      blink::MediaDeviceType type = static_cast<blink::MediaDeviceType>(i);
      host_->AddMediaDevicesListener(
          type == blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT,
          type == blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT,
          type == blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT,
          device_change_listener.CreatePendingRemoteAndBind());
      blink::WebMediaDeviceInfoArray changed_devices;
      EXPECT_CALL(device_change_listener, OnDevicesChanged(type, _))
          .WillRepeatedly(SaveArg<1>(&changed_devices));

      // Simulate device-change notification
      media_stream_manager_->media_devices_manager()->OnDevicesChanged(
          base::SystemMonitor::DEVTYPE_AUDIO);
      media_stream_manager_->media_devices_manager()->OnDevicesChanged(
          base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
      base::RunLoop().RunUntilIdle();

      if (has_permission)
        EXPECT_TRUE(DoesContainLabels(changed_devices));
      else
        EXPECT_TRUE(DoesNotContainLabels(changed_devices));
    }
  }

  MediaDeviceSaltAndOrigin GetSaltAndOrigin(int /* process_id */,
                                            int /* frame_id */) {
    return GetMediaDeviceSaltAndOrigin(-1, -1);
  }

  // The order of these members is important on teardown:
  // MediaDevicesDispatcherHost expects to be destroyed on the IO thread while
  // MediaStreamManager expects to be destroyed after the IO thread has been
  // uninitialized.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MediaDevicesDispatcherHost> host_;

  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  media::FakeVideoCaptureDeviceFactory* video_capture_device_factory_;
  MediaDeviceEnumeration physical_devices_;
  url::Origin origin_;

  std::vector<blink::WebMediaDeviceInfoArray> enumerated_devices_;
};

TEST_P(MediaDevicesDispatcherHostTest, EnumerateAudioInputDevices) {
  EnumerateDevicesAndWaitForResult(true, false, false);
  EXPECT_TRUE(DoesContainLabels(enumerated_devices_));
}

TEST_P(MediaDevicesDispatcherHostTest, EnumerateVideoInputDevices) {
  EnumerateDevicesAndWaitForResult(false, true, false);
  EXPECT_TRUE(DoesContainLabels(enumerated_devices_));
}

TEST_P(MediaDevicesDispatcherHostTest, EnumerateAudioOutputDevices) {
  EnumerateDevicesAndWaitForResult(false, false, true);
  EXPECT_TRUE(DoesContainLabels(enumerated_devices_));
}

TEST_P(MediaDevicesDispatcherHostTest, EnumerateAllDevices) {
  EnumerateDevicesAndWaitForResult(true, true, true);
  EXPECT_TRUE(DoesContainLabels(enumerated_devices_));
}

TEST_P(MediaDevicesDispatcherHostTest, EnumerateAudioInputDevicesNoAccess) {
  EnumerateDevicesAndWaitForResult(true, false, false, false);
  EXPECT_TRUE(DoesNotContainLabels(enumerated_devices_));
}

TEST_P(MediaDevicesDispatcherHostTest, EnumerateVideoInputDevicesNoAccess) {
  EnumerateDevicesAndWaitForResult(false, true, false, false);
  EXPECT_TRUE(DoesNotContainLabels(enumerated_devices_));
}

TEST_P(MediaDevicesDispatcherHostTest, EnumerateAudioOutputDevicesNoAccess) {
  EnumerateDevicesAndWaitForResult(false, false, true, false);
  EXPECT_TRUE(DoesNotContainLabels(enumerated_devices_));
}

TEST_P(MediaDevicesDispatcherHostTest, EnumerateAllDevicesNoAccess) {
  EnumerateDevicesAndWaitForResult(true, true, true, false);
  EXPECT_TRUE(DoesNotContainLabels(enumerated_devices_));
}

TEST_P(MediaDevicesDispatcherHostTest, SubscribeDeviceChange) {
  SubscribeAndWaitForResult(true);
}

TEST_P(MediaDevicesDispatcherHostTest, SubscribeDeviceChangeNoAccess) {
  SubscribeAndWaitForResult(false);
}

TEST_P(MediaDevicesDispatcherHostTest, GetVideoInputCapabilities) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, MockVideoInputCapabilitiesCallback())
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  host_->GetVideoInputCapabilities(base::BindOnce(
      &MediaDevicesDispatcherHostTest::VideoInputCapabilitiesCallback,
      base::Unretained(this)));
  run_loop.Run();
}

TEST_P(MediaDevicesDispatcherHostTest, GetAudioInputCapabilities) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, MockAudioInputCapabilitiesCallback())
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  host_->GetAudioInputCapabilities(base::BindOnce(
      &MediaDevicesDispatcherHostTest::AudioInputCapabilitiesCallback,
      base::Unretained(this)));
  run_loop.Run();
}

TEST_P(MediaDevicesDispatcherHostTest, GetAllVideoInputDeviceFormats) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, MockAllVideoInputDeviceFormatsCallback())
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  MediaDeviceSaltAndOrigin salt_and_origin =
      GetMediaDeviceSaltAndOrigin(-1, -1);
  host_->GetAllVideoInputDeviceFormats(
      GetHMACForMediaDeviceID(salt_and_origin.device_id_salt,
                              salt_and_origin.origin, kDefaultVideoDeviceID),
      base::BindOnce(
          &MediaDevicesDispatcherHostTest::AllVideoInputDeviceFormatsCallback,
          base::Unretained(this)));
  run_loop.Run();
}

TEST_P(MediaDevicesDispatcherHostTest, GetAvailableVideoInputDeviceFormats) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, MockAvailableVideoInputDeviceFormatsCallback())
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  MediaDeviceSaltAndOrigin salt_and_origin =
      GetMediaDeviceSaltAndOrigin(-1, -1);
  host_->GetAvailableVideoInputDeviceFormats(
      GetHMACForMediaDeviceID(salt_and_origin.device_id_salt,
                              salt_and_origin.origin, kNormalVideoDeviceID),
      base::BindOnce(&MediaDevicesDispatcherHostTest::
                         AvailableVideoInputDeviceFormatsCallback,
                     base::Unretained(this)));
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(,
                         MediaDevicesDispatcherHostTest,
                         testing::Values(GURL(), GURL("https://test.com")));
}  // namespace content
