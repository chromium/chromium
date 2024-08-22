// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_dispatcher_host.h"

#include <stddef.h>

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/fake_video_capture_provider.h"
#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/media/video_capture_provider_switcher.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
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
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom.h"
#include "url/origin.h"

using blink::mojom::MediaDeviceType;
using testing::_;
using testing::SaveArg;
using testing::InvokeWithoutArgs;

namespace content {

namespace {

const size_t kNumFakeVideoDevices = 3;
const char kNormalVideoDeviceID[] = "/dev/video0";
const char kNoFormatsVideoDeviceID[] = "/dev/video1";
const char kZeroResolutionVideoDeviceID[] = "/dev/video2";
const char* const kDefaultVideoDeviceID = kZeroResolutionVideoDeviceID;
const char kDefaultAudioDeviceID[] = "fake_audio_input_2";

void PhysicalDevicesEnumerated(base::OnceClosure quit_closure,
                               MediaDeviceEnumeration* out,
                               const MediaDeviceEnumeration& enumeration) {
  *out = enumeration;
  std::move(quit_closure).Run();
}

class MockMediaDevicesListener : public blink::mojom::MediaDevicesListener {
 public:
  MockMediaDevicesListener() = default;

  MOCK_METHOD2(OnDevicesChanged,
               void(MediaDeviceType, const blink::WebMediaDeviceInfoArray&));

  mojo::PendingRemote<blink::mojom::MediaDevicesListener>
  CreatePendingRemoteAndBind() {
    mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener;
    receivers_.Add(this, listener.InitWithNewPipeAndPassReceiver());
    return listener;
  }

 private:
  mojo::ReceiverSet<blink::mojom::MediaDevicesListener> receivers_;
};

std::u16string MaxLengthCaptureHandle() {
  static_assert(sizeof(std::u16string::value_type) == 2, "");
  std::u16string maxHandle = u"0123456789abcdef";  // 16 characters.
  maxHandle.reserve(1024);
  while (maxHandle.length() < 1024) {
    maxHandle += maxHandle;
  }
  CHECK_EQ(maxHandle.length(), 1024u) << "Malformed test.";
  return maxHandle;
}

class FakeContentBrowserClient : public ContentBrowserClient {
 public:
  FakeContentBrowserClient() = default;

  void PreferenceRankAudioDeviceInfos(
      BrowserContext* browser_context,
      blink::WebMediaDeviceInfoArray& infos) override {
    PreferenceRankDeviceInfos(browser_context, kDefaultAudioDeviceID, infos);
  }

  void PreferenceRankVideoDeviceInfos(
      BrowserContext* browser_context,
      blink::WebMediaDeviceInfoArray& infos) override {
    PreferenceRankDeviceInfos(browser_context, kDefaultVideoDeviceID, infos);
  }

  void set_expected_browser_context(BrowserContext* browser_context) {
    expected_browser_context_ = browser_context;
  }

 private:
  void PreferenceRankDeviceInfos(BrowserContext* browser_context,
                                 const std::string& default_device_id,
                                 blink::WebMediaDeviceInfoArray& infos) {
    CHECK(expected_browser_context_ == browser_context);
    const auto iter = std::find_if(infos.begin(), infos.end(),
                                   [default_device_id](const auto& info) {
                                     return info.device_id == default_device_id;
                                   });
    CHECK(iter < infos.end());
    auto default_device = *iter;
    infos.erase(iter);
    infos.insert(infos.begin(), default_device);
  }

  raw_ptr<BrowserContext> expected_browser_context_;
};

}  // namespace

class MediaDevicesDispatcherHostTest
    : public testing::TestWithParam<std::string> {
 public:
  MediaDevicesDispatcherHostTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        origin_(url::Origin::Create(GURL(GetParam()))) {
    // Make sure we use fake devices to avoid long delays.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());

    auto video_capture_device_factory =
        std::make_unique<media::FakeVideoCaptureDeviceFactory>();
    video_capture_device_factory_ = video_capture_device_factory.get();
    auto fake_video_capture_provider =
        std::make_unique<FakeVideoCaptureProvider>(
            std::move(video_capture_device_factory));
    auto screencapture_video_capture_provider =
        InProcessVideoCaptureProvider::CreateInstanceForScreenCapture(
            base::SingleThreadTaskRunner::GetCurrentDefault());
    auto video_capture_provider_switcher =
        std::make_unique<VideoCaptureProviderSwitcher>(
            std::move(fake_video_capture_provider),
            std::move(screencapture_video_capture_provider));

    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), std::move(video_capture_provider_switcher));

    InitializeRenderFrameHost();
    host_ = std::make_unique<MediaDevicesDispatcherHost>(
        render_frame_host_->GetGlobalId(), media_stream_manager_.get());
    media_stream_manager_->media_devices_manager()
        ->set_get_salt_and_origin_cb_for_testing(base::BindRepeating(
            &MediaDevicesDispatcherHostTest::GetSaltAndOrigin,
            base::Unretained(this)));
    host_->SetBadMessageCallbackForTesting(
        base::BindRepeating(&MediaDevicesDispatcherHostTest::MockOnBadMessage,
                            base::Unretained(this)));
    host_->SetCaptureHandleConfigCallbackForTesting(base::BindRepeating(
        &MediaDevicesDispatcherHostTest::OnCaptureHandleConfigAccepted,
        base::Unretained(this)));
  }
  ~MediaDevicesDispatcherHostTest() override {
    audio_manager_->Shutdown();
    EXPECT_FALSE(expected_set_capture_handle_config_);
    browser_client_.set_expected_browser_context(nullptr);
  }

  void SetUp() override {
    SetBrowserClientForTesting(&browser_client_);
    browser_client_.set_expected_browser_context(&browser_context_);
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
    expected_video_capture_formats_ = {
        media::VideoCaptureFormat(gfx::Size(640, 480), 30.0,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(800, 600), 30.0,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(1020, 780), 30.0,
                                  media::PIXEL_FORMAT_I420),
        media::VideoCaptureFormat(gfx::Size(1920, 1080), 20.0,
                                  media::PIXEL_FORMAT_I420)};
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
    devices_to_enumerate[static_cast<size_t>(
        MediaDeviceType::kMediaAudioInput)] = true;
    devices_to_enumerate[static_cast<size_t>(
        MediaDeviceType::kMediaVideoInput)] = true;
    devices_to_enumerate[static_cast<size_t>(
        MediaDeviceType::kMediaAudioOutput)] = true;
    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&PhysicalDevicesEnumerated, run_loop.QuitClosure(),
                       &physical_devices_));
    run_loop.Run();

    ASSERT_GT(physical_devices_[static_cast<size_t>(
                                    MediaDeviceType::kMediaAudioInput)]
                  .size(),
              0u);
    ASSERT_GT(physical_devices_[static_cast<size_t>(
                                    MediaDeviceType::kMediaVideoInput)]
                  .size(),
              0u);
    ASSERT_GT(physical_devices_[static_cast<size_t>(
                                    MediaDeviceType::kMediaAudioOutput)]
                  .size(),
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
  MOCK_METHOD2(MockOnBadMessage, void(int, bad_message::BadMessageReason));

  void OnCaptureHandleConfigAccepted(
      int render_process_id,
      int render_frame_id,
      blink::mojom::CaptureHandleConfigPtr config) {
    ASSERT_TRUE(expected_set_capture_handle_config_.has_value());

    EXPECT_EQ(render_process_id,
              expected_set_capture_handle_config_->render_process_id);
    EXPECT_EQ(render_frame_id,
              expected_set_capture_handle_config_->render_frame_id);
    EXPECT_EQ(config, expected_set_capture_handle_config_->config);

    expected_set_capture_handle_config_ = std::nullopt;
  }

  void ExpectOnCaptureHandleConfigAccepted(
      int render_process_id,
      int render_frame_id,
      blink::mojom::CaptureHandleConfigPtr config) {
    ASSERT_FALSE(expected_set_capture_handle_config_);
    expected_set_capture_handle_config_.emplace();
    expected_set_capture_handle_config_->render_process_id = render_process_id;
    expected_set_capture_handle_config_->render_frame_id = render_frame_id;
    expected_set_capture_handle_config_->config = std::move(config);
  }

  void ExpectVideoCaptureFormats(
      const std::vector<media::VideoCaptureFormat>& formats) {
    expected_video_capture_formats_ = formats;
  }

  void VideoInputCapabilitiesCallback(
      std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr> capabilities) {
    MockVideoInputCapabilitiesCallback();
    base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
    GetMediaDeviceSaltAndOrigin(GlobalRenderFrameHostId(-1, -1),
                                future.GetCallback());
    MediaDeviceSaltAndOrigin salt_and_origin = future.Get();
    std::string expected_first_device_id =
        GetHMACForRawMediaDeviceID(salt_and_origin, kDefaultVideoDeviceID);
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
    base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
    GetMediaDeviceSaltAndOrigin(GlobalRenderFrameHostId(-1, -1),
                                future.GetCallback());
    MediaDeviceSaltAndOrigin salt_and_origin = future.Get();
    std::string expected_first_device_id =
        GetHMACForRawMediaDeviceID(salt_and_origin, kDefaultAudioDeviceID);
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
    EXPECT_EQ(formats, expected_video_capture_formats_);
  }

 protected:
  void DevicesEnumerated(
      base::OnceClosure closure_after,
      const std::vector<std::vector<blink::WebMediaDeviceInfo>>& devices,
      std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr>
          video_input_capabilities,
      std::vector<blink::mojom::AudioInputDeviceCapabilitiesPtr>
          audio_input_capabilities) {
    enumerated_devices_ = devices;
    std::move(closure_after).Run();
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
      EXPECT_FALSE(enumerated_devices_[static_cast<size_t>(
                                           MediaDeviceType::kMediaAudioInput)]
                       .empty());
    if (enumerate_video_input)
      EXPECT_FALSE(enumerated_devices_[static_cast<size_t>(
                                           MediaDeviceType::kMediaVideoInput)]
                       .empty());
    if (enumerate_audio_output)
      EXPECT_FALSE(enumerated_devices_[static_cast<size_t>(
                                           MediaDeviceType::kMediaAudioOutput)]
                       .empty());

    EXPECT_FALSE(DoesContainRawIds(enumerated_devices_));
    EXPECT_EQ(DoesEveryDeviceMapToRawId(enumerated_devices_),
              permission_override_value);
  }

  bool DoesContainRawIds(
      const std::vector<std::vector<blink::WebMediaDeviceInfo>>& enumeration) {
    for (size_t i = 0;
         i < static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes); ++i) {
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
      const std::vector<std::vector<blink::WebMediaDeviceInfo>>& enumeration) {
    base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
    GetMediaDeviceSaltAndOrigin(GlobalRenderFrameHostId(-1, -1),
                                future.GetCallback());
    MediaDeviceSaltAndOrigin salt_and_origin = future.Get();
    for (size_t i = 0;
         i < static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes); ++i) {
      for (const auto& device_info : enumeration[i]) {
        bool found_match = false;
        for (const auto& raw_device_info : physical_devices_[i]) {
          if (DoesRawMediaDeviceIDMatchHMAC(salt_and_origin,
                                            device_info.device_id,
                                            raw_device_info.device_id)) {
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
    for (size_t i = 0;
         i < static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes); ++i) {
      MediaDeviceType type = static_cast<MediaDeviceType>(i);
      host_->AddMediaDevicesListener(
          type == MediaDeviceType::kMediaAudioInput,
          type == MediaDeviceType::kMediaVideoInput,
          type == MediaDeviceType::kMediaAudioOutput,
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

  void GetSaltAndOrigin(GlobalRenderFrameHostId /* render_frame_host_id */,
                        MediaDeviceSaltAndOriginCallback callback) {
    GetMediaDeviceSaltAndOrigin(GlobalRenderFrameHostId(-1, -1),
                                std::move(callback));
  }

  void InitializeRenderFrameHost() {
    web_contents_ = TestWebContents::Create(
        &browser_context_, SiteInstanceImpl::Create(&browser_context_));
    render_frame_host_ = web_contents_->GetPrimaryMainFrame();
  }

  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  // The order of these members is important on teardown:
  // MediaDevicesDispatcherHost expects to be destroyed on the IO thread while
  // MediaStreamManager expects to be destroyed after the IO thread has been
  // uninitialized.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MediaDevicesDispatcherHost> host_;

  raw_ptr<media::FakeVideoCaptureDeviceFactory> video_capture_device_factory_;
  MediaDeviceEnumeration physical_devices_;
  url::Origin origin_;

  std::vector<blink::WebMediaDeviceInfoArray> enumerated_devices_;

  struct ExpectedCaptureHandleConfig {
    int render_process_id;
    int render_frame_id;
    blink::mojom::CaptureHandleConfigPtr config;
  };
  std::optional<ExpectedCaptureHandleConfig>
      expected_set_capture_handle_config_;
  std::vector<media::VideoCaptureFormat> expected_video_capture_formats_;
  RenderViewHostTestEnabler rvh_test_enabler_;
  FakeContentBrowserClient browser_client_;
  TestBrowserContext browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  raw_ptr<TestRenderFrameHost> render_frame_host_;
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
  base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
  GetMediaDeviceSaltAndOrigin(GlobalRenderFrameHostId(-1, -1),
                              future.GetCallback());
  MediaDeviceSaltAndOrigin salt_and_origin = future.Get();
  host_->GetAllVideoInputDeviceFormats(
      GetHMACForRawMediaDeviceID(salt_and_origin, kDefaultVideoDeviceID),
      base::BindOnce(
          &MediaDevicesDispatcherHostTest::AllVideoInputDeviceFormatsCallback,
          base::Unretained(this)));
  run_loop.Run();
}

TEST_P(MediaDevicesDispatcherHostTest, GetAvailableVideoInputDeviceFormats) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, MockAvailableVideoInputDeviceFormatsCallback())
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
  GetMediaDeviceSaltAndOrigin(GlobalRenderFrameHostId(-1, -1),
                              future.GetCallback());
  MediaDeviceSaltAndOrigin salt_and_origin = future.Get();
  host_->GetAvailableVideoInputDeviceFormats(
      GetHMACForRawMediaDeviceID(salt_and_origin, kNormalVideoDeviceID),
      base::BindOnce(&MediaDevicesDispatcherHostTest::
                         AvailableVideoInputDeviceFormatsCallback,
                     base::Unretained(this)));
  run_loop.Run();
}

TEST_P(MediaDevicesDispatcherHostTest, SetCaptureHandleConfigWithNullptr) {
  EXPECT_CALL(*this,
              MockOnBadMessage(render_frame_host_->GetGlobalId().child_id,
                               bad_message::MDDH_NULL_CAPTURE_HANDLE_CONFIG));
  host_->SetCaptureHandleConfig(nullptr);
}

TEST_P(MediaDevicesDispatcherHostTest,
       SetCaptureHandleConfigWithExcessivelLongHandle) {
  auto config = blink::mojom::CaptureHandleConfig::New();
  config->capture_handle = MaxLengthCaptureHandle() + u"a";  // Max exceeded.
  EXPECT_CALL(*this,
              MockOnBadMessage(render_frame_host_->GetGlobalId().child_id,
                               bad_message::MDDH_INVALID_CAPTURE_HANDLE));
  host_->SetCaptureHandleConfig(std::move(config));
}

TEST_P(MediaDevicesDispatcherHostTest,
       SetCaptureHandleConfigWithAllPermittedAndSpecificallyPermitted) {
  auto config = blink::mojom::CaptureHandleConfig::New();
  config->all_origins_permitted = true;
  config->permitted_origins = {
      url::Origin::Create(GURL("https://chromium.org:123"))};
  EXPECT_CALL(
      *this, MockOnBadMessage(render_frame_host_->GetGlobalId().child_id,
                              bad_message::MDDH_INVALID_ALL_ORIGINS_PERMITTED));
  host_->SetCaptureHandleConfig(std::move(config));
}

TEST_P(MediaDevicesDispatcherHostTest, SetCaptureHandleConfigWithBadOrigin) {
  auto config = blink::mojom::CaptureHandleConfig::New();
  config->permitted_origins = {
      url::Origin::Create(GURL("https://chromium.org:999999"))  // Invalid.
  };
  EXPECT_CALL(*this,
              MockOnBadMessage(render_frame_host_->GetGlobalId().child_id,
                               bad_message::MDDH_INVALID_PERMITTED_ORIGIN));
  host_->SetCaptureHandleConfig(std::move(config));
}

TEST_P(MediaDevicesDispatcherHostTest,
       SetCaptureHandleConfigWithMaxHandleLengthAllowed) {
  auto config = blink::mojom::CaptureHandleConfig::New();
  // Valid (and max-length) handle.
  config->capture_handle = MaxLengthCaptureHandle();
  config->permitted_origins = {
      url::Origin::Create(GURL("https://chromium.org:123")),
      url::Origin::Create(GURL("ftp://google.com:321"))};
  EXPECT_CALL(*this, MockOnBadMessage(_, _)).Times(0);
  ExpectOnCaptureHandleConfigAccepted(
      render_frame_host_->GetGlobalId().child_id,
      render_frame_host_->GetGlobalId().frame_routing_id, config->Clone());
  host_->SetCaptureHandleConfig(std::move(config));
}

TEST_P(MediaDevicesDispatcherHostTest,
       SetCaptureHandleConfigWithSpecificOriginsAllowed) {
  auto config = blink::mojom::CaptureHandleConfig::New();
  config->capture_handle = u"0123456789abcdef";
  config->permitted_origins = {
      url::Origin::Create(GURL("https://chromium.org:123")),
      url::Origin::Create(GURL("ftp://google.com:321"))};
  EXPECT_CALL(*this, MockOnBadMessage(_, _)).Times(0);
  ExpectOnCaptureHandleConfigAccepted(
      render_frame_host_->GetGlobalId().child_id,
      render_frame_host_->GetGlobalId().frame_routing_id, config->Clone());
  host_->SetCaptureHandleConfig(std::move(config));
}

TEST_P(MediaDevicesDispatcherHostTest,
       SetCaptureHandleConfigWithAllOriginsAllowed) {
  EXPECT_CALL(*this, MockOnBadMessage(_, _)).Times(0);
  auto config = blink::mojom::CaptureHandleConfig::New();
  config->capture_handle = u"0123456789abcdef";
  config->all_origins_permitted = true;
  EXPECT_CALL(*this, MockOnBadMessage(_, _)).Times(0);
  ExpectOnCaptureHandleConfigAccepted(
      render_frame_host_->GetGlobalId().child_id,
      render_frame_host_->GetGlobalId().frame_routing_id, config->Clone());
  host_->SetCaptureHandleConfig(std::move(config));
}

TEST_P(MediaDevicesDispatcherHostTest,
       GetAvailableVideoInputDeviceFormatsUnfoundDevice) {
  base::RunLoop run_loop;
  // Expect an empty list of supported formats for an unfound device.
  ExpectVideoCaptureFormats({});
  EXPECT_CALL(*this, MockAvailableVideoInputDeviceFormatsCallback())
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  host_->GetAvailableVideoInputDeviceFormats(
      "UnknownHashedDeviceId",
      base::BindOnce(&MediaDevicesDispatcherHostTest::
                         AvailableVideoInputDeviceFormatsCallback,
                     base::Unretained(this)));
  run_loop.Run();
}

TEST_P(MediaDevicesDispatcherHostTest,
       GetAllVideoInputDeviceFormatsUnfoundDevice) {
  base::RunLoop run_loop;
  // Expect an empty list of supported formats for an unfound device.
  ExpectVideoCaptureFormats({});
  EXPECT_CALL(*this, MockAvailableVideoInputDeviceFormatsCallback())
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  host_->GetAllVideoInputDeviceFormats(
      "UnknownHashedDeviceId",
      base::BindOnce(&MediaDevicesDispatcherHostTest::
                         AvailableVideoInputDeviceFormatsCallback,
                     base::Unretained(this)));
  run_loop.Run();
}

TEST_P(MediaDevicesDispatcherHostTest,
       RegisterAndUnregisterWithMediaDevicesManager) {
  {
    mojo::Remote<blink::mojom::MediaDevicesDispatcherHost> client;
    MediaDevicesDispatcherHost::Create(render_frame_host_->GetGlobalId(),
                                       media_stream_manager_.get(),
                                       client.BindNewPipeAndPassReceiver());
    EXPECT_TRUE(client.is_bound());
    EXPECT_EQ(media_stream_manager_->media_devices_manager()
                  ->num_registered_dispatcher_hosts(),
              1u);
  }
  task_environment_.RunUntilIdle();
  // At this point the dispatcher created by MediaDevicesDispatcherHost::Create
  // should be destroyed and unregistered from MediaDevicesManager.
  EXPECT_EQ(media_stream_manager_->media_devices_manager()
                ->num_registered_dispatcher_hosts(),
            0u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaDevicesDispatcherHostTest,
                         testing::Values(std::string(), "https://test.com"));
}  // namespace content
