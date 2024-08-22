// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_manager.h"

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/system_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/mock_video_capture_provider.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_web_contents.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/video_facing.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::HistogramTester;
using blink::mojom::MediaDeviceType;
using media::mojom::DeviceEnumerationResult;
using media::mojom::SubCaptureTargetType;
using ::testing::_;
using ::testing::Invoke;
using ::testing::SaveArg;

namespace content {

namespace {

const int kRenderProcessId = 1;
const int kRenderFrameId = 3;
const GlobalRenderFrameHostId kRenderFrameHostId{kRenderProcessId,
                                                 kRenderFrameId};

// Number of client enumerations to simulate on each test run.
// This allows testing that a single call to low-level enumeration functions
// is performed when cache is enabled, regardless of the number of client calls.
const int kNumCalls = 3;

const size_t kNumAudioInputDevices = 2;

const auto kIgnoreLogMessageCB = base::DoNothing();

std::string salt = "fake_media_device_salt";
void GetSaltAndOrigin(GlobalRenderFrameHostId,
                      MediaDeviceSaltAndOriginCallback callback) {
  std::move(callback).Run(MediaDeviceSaltAndOrigin(
      salt, url::Origin::Create(GURL("https://test.com")), "fake_group_id_salt",
      /*has_focus=*/true, /*is_background=*/false));
}

std::string GetAudioDeviceName(size_t suffix) {
  return "fake_device_name_" + base::NumberToString(suffix);
}

std::string GetAudioDeviceId(size_t suffix) {
  return "fake_device_id_" + base::NumberToString(suffix);
}

std::string GetVideoDeviceName(size_t suffix) {
  return "fake_device_" + base::NumberToString(suffix);
}

std::string GetVideoDeviceId(size_t suffix) {
  return "/dev/video" + base::NumberToString(suffix);
}

// This class mocks the audio manager and overrides some methods to ensure that
// we can run simulate device changes.
class MockAudioManager : public media::FakeAudioManager {
 public:
  MockAudioManager()
      : FakeAudioManager(std::make_unique<media::TestAudioThread>(),
                         &fake_audio_log_factory_),
        num_output_devices_(2),
        num_input_devices_(kNumAudioInputDevices) {}

  MockAudioManager(const MockAudioManager&) = delete;
  MockAudioManager& operator=(const MockAudioManager&) = delete;

  ~MockAudioManager() override {}

  MOCK_METHOD1(MockGetAudioInputDeviceNames, void(media::AudioDeviceNames*));
  MOCK_METHOD1(MockGetAudioOutputDeviceNames, void(media::AudioDeviceNames*));

  std::string GetDefaultInputDeviceID() override { return default_device_id_; }

  std::string GetCommunicationsInputDeviceID() override {
    return communications_device_id_;
  }

  void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());
    if (default_device_id_ != std::string()) {
      device_names->push_back(media::AudioDeviceName(
          media::AudioDeviceDescription::kDefaultDeviceId,
          media::AudioDeviceDescription::kDefaultDeviceId));
    }
    if (communications_device_id_ != std::string()) {
      device_names->push_back(media::AudioDeviceName(
          std::string(),
          media::AudioDeviceDescription::kCommunicationsDeviceId));
    }

    size_t num_devices_to_create = num_input_devices_;
    size_t start_id_trailer = 0;
    while (num_devices_to_create > 0) {
      size_t trailer = start_id_trailer++;
      std::string id("fake_device_id_" + base::NumberToString(trailer));
      if (base::Contains(removed_input_audio_device_ids_, id))
        continue;

      device_names->push_back(media::AudioDeviceName(
          std::string("fake_device_name_") + base::NumberToString(trailer),
          id));
      --num_devices_to_create;
    }
    MockGetAudioInputDeviceNames(device_names);
  }

  void GetAudioOutputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());
    for (size_t i = 0; i < num_output_devices_; i++) {
      device_names->push_back(media::AudioDeviceName(
          std::string("fake_device_name_") + base::NumberToString(i),
          std::string("fake_device_id_") + base::NumberToString(i)));
    }
    MockGetAudioOutputDeviceNames(device_names);
  }

  media::AudioParameters GetOutputStreamParameters(
      const std::string& device_id) override {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::ChannelLayoutConfig::Stereo(), 48000,
                                  128);
  }

  void RemoveInputAudioDeviceById(const std::string& device_id) {
    --num_input_devices_;
    removed_input_audio_device_ids_.insert(device_id);
    if (device_id == default_device_id_)
      default_device_id_ = std::string();
    if (device_id == communications_device_id_)
      communications_device_id_ = std::string();
  }

  void SetNumAudioOutputDevices(size_t num_devices) {
    num_output_devices_ = num_devices;
  }

  void SetNumAudioInputDevices(size_t num_devices) {
    num_input_devices_ = num_devices;
  }

  void SetDefaultDeviceToId(const std::string& device_id) {
    default_device_id_ = device_id;
  }

  void SetCommunicationsDeviceToId(const std::string& device_id) {
    communications_device_id_ = device_id;
  }

 private:
  media::FakeAudioLogFactory fake_audio_log_factory_;
  size_t num_output_devices_;
  size_t num_input_devices_;
  std::string default_device_id_;
  std::string communications_device_id_;
  std::set<std::string> removed_input_audio_device_ids_;
};

// This class mocks the video capture device factory and overrides some methods
// to ensure that we can simulate device changes.
class MockVideoCaptureDeviceFactory
    : public media::FakeVideoCaptureDeviceFactory {
 public:
  MockVideoCaptureDeviceFactory() {}
  ~MockVideoCaptureDeviceFactory() override {}

  MOCK_METHOD0(MockGetDevicesInfo, void());
  void GetDevicesInfo(GetDevicesInfoCallback callback) override {
    media::FakeVideoCaptureDeviceFactory::GetDevicesInfo(std::move(callback));
    MockGetDevicesInfo();
  }
};

class MockMediaDevicesListener : public blink::mojom::MediaDevicesListener {
 public:
  MockMediaDevicesListener() {}

  MOCK_METHOD2(OnDevicesChanged,
               void(MediaDeviceType, const blink::WebMediaDeviceInfoArray&));

  mojo::PendingRemote<blink::mojom::MediaDevicesListener>
  CreateInterfacePtrAndBind() {
    mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener;
    receivers_.Add(this, listener.InitWithNewPipeAndPassReceiver());
    return listener;
  }

 private:
  mojo::ReceiverSet<blink::mojom::MediaDevicesListener> receivers_;
};

class MockMediaDevicesManagerClient {
 public:
  MOCK_METHOD2(StopRemovedInputDevice,
               void(MediaDeviceType type,
                    const blink::WebMediaDeviceInfo& media_device_info));
  MOCK_METHOD2(InputDevicesChangedUI,
               void(MediaDeviceType stream_type,
                    const blink::WebMediaDeviceInfoArray& devices));
};

void VerifyDeviceAndGroupID(
    const std::vector<blink::WebMediaDeviceInfoArray>& array) {
  for (const auto& device_infos : array) {
    for (const auto& device_info : device_infos) {
      EXPECT_FALSE(device_info.device_id.empty());
      EXPECT_FALSE(device_info.group_id.empty());
    }
  }
}

class MockMediaDevicesDispatcherHost
    : public blink::mojom::MediaDevicesDispatcherHost {
  MOCK_METHOD(void,
              EnumerateDevices,
              (bool request_audio_input,
               bool request_video_input,
               bool request_audio_output,
               bool request_video_input_capabilities,
               bool request_audio_input_capabilities,
               EnumerateDevicesCallback callback));
  MOCK_METHOD(void,
              GetVideoInputCapabilities,
              (GetVideoInputCapabilitiesCallback callback));
  MOCK_METHOD(void,
              GetAllVideoInputDeviceFormats,
              (const std::string& device_id,
               GetAllVideoInputDeviceFormatsCallback callback));
  MOCK_METHOD(void,
              GetAvailableVideoInputDeviceFormats,
              (const std::string& device_id,
               GetAvailableVideoInputDeviceFormatsCallback callback));
  MOCK_METHOD(void,
              GetAudioInputCapabilities,
              (GetAudioInputCapabilitiesCallback callback));
  MOCK_METHOD(
      void,
      AddMediaDevicesListener,
      (bool subscribe_audio_input,
       bool subscribe_video_input,
       bool subscribe_audio_output,
       mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener));
  MOCK_METHOD(void,
              SetCaptureHandleConfig,
              (::blink::mojom::CaptureHandleConfigPtr config));
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  MOCK_METHOD(void, CloseFocusWindowOfOpportunity, (const std::string& label));
  MOCK_METHOD(void,
              ProduceSubCaptureTargetId,
              (SubCaptureTargetType type,
               ProduceSubCaptureTargetIdCallback callback));
#endif
};

class MockBrowserClient : public ContentBrowserClient {
 public:
  MOCK_METHOD(void,
              PreferenceRankAudioDeviceInfos,
              (BrowserContext * browser_context,
               blink::WebMediaDeviceInfoArray& infos),
              (override));
  MOCK_METHOD(void,
              PreferenceRankVideoDeviceInfos,
              (BrowserContext * browser_context,
               blink::WebMediaDeviceInfoArray& infos),
              (override));
};

}  // namespace

class MediaDevicesManagerTest : public ::testing::Test {
 public:
  MediaDevicesManagerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  MediaDevicesManagerTest(const MediaDevicesManagerTest&) = delete;
  MediaDevicesManagerTest& operator=(const MediaDevicesManagerTest&) = delete;

  ~MediaDevicesManagerTest() override { audio_manager_->Shutdown(); }

  MOCK_METHOD1(MockCallback, void(const MediaDeviceEnumeration&));

  void TrackRemovedDevice(MediaDeviceType type,
                          const blink::WebMediaDeviceInfo& media_device_info) {
    removed_device_ids_.insert(media_device_info.device_id);
  }

  void RunEnumerateDevices() {
    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[static_cast<size_t>(
        MediaDeviceType::kMediaAudioInput)] = true;

    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
  }

  void EnumerateCallback(base::RunLoop* run_loop,
                         const MediaDeviceEnumeration& result) {
    for (int i = 0; i < static_cast<int>(MediaDeviceType::kNumMediaDeviceTypes);
         ++i) {
      for (const auto& device_info : result[i]) {
        EXPECT_FALSE(device_info.device_id.empty());
        EXPECT_FALSE(device_info.group_id.empty());
      }
    }
    MockCallback(result);
    run_loop->Quit();
  }

  void EnumerateWithCapabilitiesCallback(
      const std::vector<media::FakeVideoCaptureDeviceSettings>&
          expected_video_capture_device_settings,
      base::RunLoop* run_loop,
      const std::vector<blink::WebMediaDeviceInfoArray>& devices,
      std::vector<VideoInputDeviceCapabilitiesPtr> video_capabilities,
      std::vector<AudioInputDeviceCapabilitiesPtr> audio_capabilities) {
    EXPECT_EQ(video_capabilities.size(),
              expected_video_capture_device_settings.size());
    for (size_t i = 0; i < video_capabilities.size(); ++i) {
      EXPECT_EQ(
          video_capabilities[i]->formats.size(),
          expected_video_capture_device_settings[i].supported_formats.size());
      for (size_t j = 0; j < video_capabilities[i]->formats.size(); ++j) {
        EXPECT_EQ(
            video_capabilities[i]->formats[j],
            expected_video_capture_device_settings[i].supported_formats[j]);
      }
      EXPECT_EQ(
          video_capabilities[i]->availability.has_value(),
          expected_video_capture_device_settings[i].availability.has_value());
      if (video_capabilities[i]->availability.has_value()) {
        EXPECT_EQ(static_cast<media::CameraAvailability>(
                      *video_capabilities[i]->availability),
                  *expected_video_capture_device_settings[i].availability);
      }
    }
    EXPECT_EQ(audio_capabilities.size(), kNumAudioInputDevices);
    for (size_t i = 0; i < audio_capabilities.size(); ++i) {
      EXPECT_TRUE(audio_capabilities[i]->parameters.IsValid());
      EXPECT_EQ(audio_capabilities[i]->parameters.channel_layout(),
                media::CHANNEL_LAYOUT_STEREO);
      EXPECT_EQ(audio_capabilities[i]->parameters.sample_rate(),
                media::AudioParameters::kAudioCDSampleRate);
      EXPECT_EQ(audio_capabilities[i]->parameters.frames_per_buffer(),
                media::AudioParameters::kAudioCDSampleRate / 100);
    }
    run_loop->Quit();
  }

 protected:
  void SetUp() override {
    SetBrowserClientForTesting(&browser_client_);
    audio_manager_ = std::make_unique<MockAudioManager>();
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    auto video_capture_device_factory =
        std::make_unique<MockVideoCaptureDeviceFactory>();
    video_capture_device_factory_ = video_capture_device_factory.get();
    video_capture_system_ = std::make_unique<media::VideoCaptureSystemImpl>(
        std::move(video_capture_device_factory));

    auto mock_video_capture_provider =
        std::make_unique<MockVideoCaptureProvider>();
    mock_video_capture_provider_ = mock_video_capture_provider.get();
    // By default, forward calls to the real video_capture_system.
    ON_CALL(*mock_video_capture_provider_, GetDeviceInfosAsync(_))
        .WillByDefault(Invoke(
            [&](VideoCaptureProvider::GetDeviceInfosCallback result_callback) {
              video_capture_system_->GetDeviceInfosAsync(base::BindOnce(
                  std::move(result_callback),
                  media::mojom::DeviceEnumerationResult::kSuccess));
            }));

    video_capture_manager_ = new VideoCaptureManager(
        std::move(mock_video_capture_provider), kIgnoreLogMessageCB);
    media_devices_manager_ = std::make_unique<MediaDevicesManager>(
        audio_system_.get(), video_capture_manager_,
        base::BindRepeating(
            &MockMediaDevicesManagerClient::StopRemovedInputDevice,
            base::Unretained(&media_devices_manager_client_)),
        base::BindRepeating(
            &MockMediaDevicesManagerClient::InputDevicesChangedUI,
            base::Unretained(&media_devices_manager_client_)));
    media_devices_manager_->set_get_salt_and_origin_cb_for_testing(
        base::BindRepeating(&GetSaltAndOrigin));
    media_devices_manager_->SetPermissionChecker(
        std::make_unique<MediaDevicesPermissionChecker>(true));
  }

  void TearDown() override { video_capture_device_factory_ = nullptr; }

  void EnableCache(MediaDeviceType type) {
    media_devices_manager_->SetCachePolicy(
        type, MediaDevicesManager::CachePolicy::SYSTEM_MONITOR);
  }

  void ExpectVideoEnumerationHistogramReport(int success_count,
                                             int error_count = 0) {
    histogram_tester_.ExpectTotalCount(
        "Media.MediaDevicesManager.VideoDeviceEnumeration.Start",
        success_count + error_count);
    histogram_tester_.ExpectBucketCount(
        "Media.MediaDevicesManager.VideoDeviceEnumeration.Result",
        DeviceEnumerationResult::kSuccess, success_count);
    histogram_tester_.ExpectBucketCount(
        "Media.MediaDevicesManager.VideoDeviceEnumeration.Result",
        DeviceEnumerationResult::kUnknownError, error_count);
  }

  void InitializeRenderFrameHost() {
    web_contents_ = TestWebContents::Create(
        &browser_context_, SiteInstanceImpl::Create(&browser_context_));
    render_frame_host_ = web_contents_->GetPrimaryMainFrame();
  }

  void FireDevicesChanged(base::SystemMonitor::DeviceType type) {
    media_devices_manager_->OnDevicesChanged(type);
    task_environment_.RunUntilIdle();
  }

  // Must outlive MediaDevicesManager as ~MediaDevicesManager() verifies it's
  // running on the IO thread.
  BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MediaDevicesManager> media_devices_manager_;
  scoped_refptr<VideoCaptureManager> video_capture_manager_;
  raw_ptr<MockVideoCaptureDeviceFactory> video_capture_device_factory_ =
      nullptr;
  std::unique_ptr<MockAudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  testing::StrictMock<MockMediaDevicesManagerClient>
      media_devices_manager_client_;
  std::set<std::string> removed_device_ids_;
  raw_ptr<MockVideoCaptureProvider> mock_video_capture_provider_ = nullptr;
  std::unique_ptr<media::VideoCaptureSystemImpl> video_capture_system_;
  HistogramTester histogram_tester_;
  RenderViewHostTestEnabler rvh_test_enabler_;
  MockBrowserClient browser_client_;
  // Must be destroyed before `browser_client_`, since `BrowserContext`
  // destruction can use the `ContentBrowserClient`.
  TestBrowserContext browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  raw_ptr<TestRenderFrameHost> render_frame_host_;
};

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheAudioInput) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_))
      .Times(kNumCalls);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
  }
  ExpectVideoEnumerationHistogramReport(0);
}

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheVideoInput) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo())
      .Times(kNumCalls);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
  }
  ExpectVideoEnumerationHistogramReport(kNumCalls);
}

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheAudioOutput) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_))
      .Times(kNumCalls);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheAudio) {
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_))
      .Times(kNumCalls);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_))
      .Times(kNumCalls);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] =
      true;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheAudioInputRanked) {
  InitializeRenderFrameHost();
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_))
      .Times(kNumCalls);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  const size_t kNumDevices = 3;
  audio_manager_->SetNumAudioInputDevices(kNumDevices);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] =
      true;

  blink::WebMediaDeviceInfoArray audio_devices;
  for (size_t i = 0; i < kNumDevices; ++i) {
    audio_devices.emplace_back(GetAudioDeviceId(i), GetAudioDeviceName(i),
                               /*group_id=*/"");
  }

  for (int i = 0; i < kNumCalls; i++) {
    EXPECT_CALL(browser_client_, PreferenceRankAudioDeviceInfos(
                                     &browser_context_, audio_devices));
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateAndRankDevices(
        render_frame_host_->GetGlobalId(), devices_to_enumerate,
        /*request_video_input_capabilities=*/false,
        /*request_audio_input_capabilities=*/true,
        base::BindLambdaForTesting(
            [&run_loop, kNumDevices](
                const std::vector<blink::WebMediaDeviceInfoArray>& devices,
                std::vector<VideoInputDeviceCapabilitiesPtr> video_capabilities,
                std::vector<AudioInputDeviceCapabilitiesPtr>
                    audio_capabilities) {
              ASSERT_EQ(kNumDevices,
                        devices[static_cast<size_t>(
                                    MediaDeviceType::kMediaAudioInput)]
                            .size());

              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateNoCacheVideoInputRanked) {
  InitializeRenderFrameHost();
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo())
      .Times(kNumCalls);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(2);
  const size_t kNumDevices = 3;
  video_capture_device_factory_->SetToDefaultDevicesConfig(kNumDevices);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;

  blink::WebMediaDeviceInfoArray video_devices;
  for (size_t i = 0; i < kNumDevices; ++i) {
    video_devices.emplace_back(media::VideoCaptureDeviceDescriptor{
        GetVideoDeviceName(i), GetVideoDeviceId(i)});
  }

  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    EXPECT_CALL(browser_client_, PreferenceRankVideoDeviceInfos(
                                     &browser_context_, video_devices));
    media_devices_manager_->EnumerateAndRankDevices(
        render_frame_host_->GetGlobalId(), devices_to_enumerate, true, false,
        base::BindLambdaForTesting(
            [&run_loop, kNumDevices](
                const std::vector<blink::WebMediaDeviceInfoArray>& devices,
                std::vector<VideoInputDeviceCapabilitiesPtr> video_capabilities,
                std::vector<AudioInputDeviceCapabilitiesPtr>
                    audio_capabilities) {
              ASSERT_EQ(kNumDevices,
                        devices[static_cast<size_t>(
                                    MediaDeviceType::kMediaVideoInput)]
                            .size());

              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheAudio) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(1);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(1);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  EnableCache(MediaDeviceType::kMediaAudioInput);
  EnableCache(MediaDeviceType::kMediaAudioOutput);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] =
      true;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheVideo) {
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(1);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  EnableCache(MediaDeviceType::kMediaVideoInput);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
  }
  ExpectVideoEnumerationHistogramReport(1);
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheAudioWithDeviceChanges) {
  MediaDeviceEnumeration enumeration;
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(3);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(3);
  EXPECT_CALL(*this, MockCallback(_))
      .Times(3 * kNumCalls)
      .WillRepeatedly(SaveArg<0>(&enumeration));
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  size_t num_audio_input_devices = 5;
  size_t num_audio_output_devices = 3;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  EnableCache(MediaDeviceType::kMediaAudioInput);
  EnableCache(MediaDeviceType::kMediaAudioOutput);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] =
      true;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(
        num_audio_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)]
            .size());
    EXPECT_EQ(
        num_audio_output_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)]
            .size());
  }

  // Simulate removal of devices.
  size_t old_num_audio_input_devices = num_audio_input_devices;
  num_audio_input_devices = 3;
  num_audio_output_devices = 2;
  ASSERT_LT(num_audio_input_devices, old_num_audio_input_devices);
  size_t num_removed_audio_input_devices =
      old_num_audio_input_devices - num_audio_input_devices;
  EXPECT_CALL(media_devices_manager_client_, StopRemovedInputDevice(_, _))
      .Times(num_removed_audio_input_devices);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));

  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(
        num_audio_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)]
            .size());
    EXPECT_EQ(
        num_audio_output_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)]
            .size());
  }

  // Simulate addition of devices.
  old_num_audio_input_devices = num_audio_input_devices;
  num_audio_input_devices = 4;
  num_audio_output_devices = 3;
  ASSERT_GT(num_audio_input_devices, old_num_audio_input_devices);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));

  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(
        num_audio_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)]
            .size());
    EXPECT_EQ(
        num_audio_output_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)]
            .size());
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheVideoWithDeviceChanges) {
  MediaDeviceEnumeration enumeration;
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(3);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_))
      .Times(3 * kNumCalls)
      .WillRepeatedly(SaveArg<0>(&enumeration));
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));

  // First enumeration.
  size_t num_video_input_devices = 5;
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      num_video_input_devices);
  EnableCache(MediaDeviceType::kMediaVideoInput);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(
        num_video_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)]
            .size());
  }

  // Simulate addition of devices.
  size_t old_num_video_input_devices = num_video_input_devices;
  num_video_input_devices = 9;
  ASSERT_GT(num_video_input_devices, old_num_video_input_devices);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      num_video_input_devices);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(
        num_video_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)]
            .size());
  }

  // Simulate removal of devices.
  old_num_video_input_devices = num_video_input_devices;
  num_video_input_devices = 7;
  ASSERT_LT(num_video_input_devices, old_num_video_input_devices);
  size_t num_removed_video_input_devices =
      old_num_video_input_devices - num_video_input_devices;
  EXPECT_CALL(media_devices_manager_client_, StopRemovedInputDevice(_, _))
      .Times(num_removed_video_input_devices);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      num_video_input_devices);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(
        num_video_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)]
            .size());
  }
}

TEST_F(MediaDevicesManagerTest, EnumerateCacheAllWithDeviceChanges) {
  MediaDeviceEnumeration enumeration;
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(2);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(2);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(2);
  EXPECT_CALL(*this, MockCallback(_))
      .Times(2 * kNumCalls)
      .WillRepeatedly(SaveArg<0>(&enumeration));
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(2);

  size_t num_audio_input_devices = 5;
  size_t num_video_input_devices = 4;
  size_t num_audio_output_devices = 3;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  EnableCache(MediaDeviceType::kMediaAudioInput);
  EnableCache(MediaDeviceType::kMediaAudioOutput);
  EnableCache(MediaDeviceType::kMediaVideoInput);
  task_environment_.RunUntilIdle();

  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] =
      true;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(
        num_audio_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)]
            .size());
    EXPECT_EQ(
        num_video_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)]
            .size());
    EXPECT_EQ(
        num_audio_output_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)]
            .size());
  }

  // Simulate device changes
  size_t old_num_audio_input_devices = num_audio_input_devices;
  size_t old_num_video_input_devices = num_video_input_devices;
  num_audio_input_devices = 3;
  num_video_input_devices = 2;
  num_audio_output_devices = 4;
  ASSERT_LT(num_audio_input_devices, old_num_audio_input_devices);
  ASSERT_LT(num_video_input_devices, old_num_video_input_devices);
  size_t num_removed_input_devices =
      old_num_audio_input_devices - num_audio_input_devices +
      old_num_video_input_devices - num_video_input_devices;
  EXPECT_CALL(media_devices_manager_client_, StopRemovedInputDevice(_, _))
      .Times(num_removed_input_devices);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(2);
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
    EXPECT_EQ(
        num_audio_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)]
            .size());
    EXPECT_EQ(
        num_video_input_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)]
            .size());
    EXPECT_EQ(
        num_audio_output_devices,
        enumeration[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)]
            .size());
  }
}

TEST_F(MediaDevicesManagerTest, SubscribeDeviceChanges) {
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(3);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(3);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(3);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(2);

  size_t num_audio_input_devices = 5;
  size_t num_video_input_devices = 4;
  size_t num_audio_output_devices = 3;
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);

  // Run an enumeration to make sure |media_devices_manager_| has the new
  // configuration.
  EXPECT_CALL(*this, MockCallback(_));
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] =
      true;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)] =
      true;
  base::RunLoop run_loop;
  media_devices_manager_->EnumerateDevices(
      devices_to_enumerate,
      base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();

  InitializeRenderFrameHost();

  // Add device-change event listeners.
  MockMediaDevicesListener listener_audio_input;
  MediaDevicesManager::BoolDeviceTypes audio_input_devices_to_subscribe;
  audio_input_devices_to_subscribe[static_cast<size_t>(
      MediaDeviceType::kMediaAudioInput)] = true;
  uint32_t audio_input_subscription_id =
      media_devices_manager_->SubscribeDeviceChangeNotifications(
          kRenderFrameHostId, audio_input_devices_to_subscribe,
          listener_audio_input.CreateInterfacePtrAndBind());

  MockMediaDevicesListener listener_video_input;
  MediaDevicesManager::BoolDeviceTypes video_input_devices_to_subscribe;
  video_input_devices_to_subscribe[static_cast<size_t>(
      MediaDeviceType::kMediaVideoInput)] = true;
  uint32_t video_input_subscription_id =
      media_devices_manager_->SubscribeDeviceChangeNotifications(
          kRenderFrameHostId, video_input_devices_to_subscribe,
          listener_video_input.CreateInterfacePtrAndBind());

  MockMediaDevicesListener listener_audio_output;
  MediaDevicesManager::BoolDeviceTypes audio_output_devices_to_subscribe;
  audio_output_devices_to_subscribe[static_cast<size_t>(
      MediaDeviceType::kMediaAudioOutput)] = true;
  uint32_t audio_output_subscription_id =
      media_devices_manager_->SubscribeDeviceChangeNotifications(
          kRenderFrameHostId, audio_output_devices_to_subscribe,
          listener_audio_output.CreateInterfacePtrAndBind());

  MockMediaDevicesListener listener_all;
  MediaDevicesManager::BoolDeviceTypes all_devices_to_subscribe;
  all_devices_to_subscribe[static_cast<size_t>(
      MediaDeviceType::kMediaAudioInput)] = true;
  all_devices_to_subscribe[static_cast<size_t>(
      MediaDeviceType::kMediaVideoInput)] = true;
  all_devices_to_subscribe[static_cast<size_t>(
      MediaDeviceType::kMediaAudioOutput)] = true;
  media_devices_manager_->SubscribeDeviceChangeNotifications(
      kRenderFrameHostId, all_devices_to_subscribe,
      listener_all.CreateInterfacePtrAndBind());

  blink::WebMediaDeviceInfoArray notification_audio_input;
  blink::WebMediaDeviceInfoArray notification_video_input;
  blink::WebMediaDeviceInfoArray notification_audio_output;
  blink::WebMediaDeviceInfoArray notification_all_audio_input;
  blink::WebMediaDeviceInfoArray notification_all_video_input;
  blink::WebMediaDeviceInfoArray notification_all_audio_output;
  EXPECT_CALL(listener_audio_input,
              OnDevicesChanged(MediaDeviceType::kMediaAudioInput, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&notification_audio_input));
  EXPECT_CALL(listener_video_input,
              OnDevicesChanged(MediaDeviceType::kMediaVideoInput, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&notification_video_input));
  EXPECT_CALL(listener_audio_output,
              OnDevicesChanged(MediaDeviceType::kMediaAudioOutput, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&notification_audio_output));
  EXPECT_CALL(listener_all,
              OnDevicesChanged(MediaDeviceType::kMediaAudioInput, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&notification_all_audio_input));
  EXPECT_CALL(listener_all,
              OnDevicesChanged(MediaDeviceType::kMediaVideoInput, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&notification_all_video_input));
  EXPECT_CALL(listener_all,
              OnDevicesChanged(MediaDeviceType::kMediaAudioOutput, _))
      .Times(2)
      .WillRepeatedly(SaveArg<1>(&notification_all_audio_output));

  // Simulate device changes.
  size_t old_num_audio_input_devices = num_audio_input_devices;
  size_t old_num_video_input_devices = num_video_input_devices;
  num_audio_input_devices = 3;
  num_video_input_devices = 2;
  num_audio_output_devices = 4;
  ASSERT_LT(num_audio_input_devices, old_num_audio_input_devices);
  ASSERT_LT(num_video_input_devices, old_num_video_input_devices);
  size_t num_removed_input_devices =
      old_num_audio_input_devices - num_audio_input_devices +
      old_num_video_input_devices - num_video_input_devices;
  EXPECT_CALL(media_devices_manager_client_, StopRemovedInputDevice(_, _))
      .Times(num_removed_input_devices);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(2);
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  EXPECT_EQ(num_audio_input_devices, notification_audio_input.size());
  EXPECT_EQ(num_video_input_devices, notification_video_input.size());
  EXPECT_EQ(num_audio_output_devices, notification_audio_output.size());
  EXPECT_EQ(num_audio_input_devices, notification_all_audio_input.size());
  EXPECT_EQ(num_video_input_devices, notification_all_video_input.size());
  EXPECT_EQ(num_audio_output_devices, notification_all_audio_output.size());
  VerifyDeviceAndGroupID(
      {notification_audio_input, notification_video_input,
       notification_audio_output, notification_all_audio_input,
       notification_all_video_input, notification_all_audio_output});

  media_devices_manager_->UnsubscribeDeviceChangeNotifications(
      audio_input_subscription_id);
  media_devices_manager_->UnsubscribeDeviceChangeNotifications(
      video_input_subscription_id);
  media_devices_manager_->UnsubscribeDeviceChangeNotifications(
      audio_output_subscription_id);

  // Simulate further device changes. Only the objects still subscribed to the
  // device-change events will receive notifications.
  old_num_audio_input_devices = num_audio_input_devices;
  old_num_video_input_devices = num_video_input_devices;
  num_audio_input_devices = 2;
  num_video_input_devices = 1;
  num_audio_output_devices = 3;
  ASSERT_LT(num_audio_input_devices, old_num_audio_input_devices);
  ASSERT_LT(num_video_input_devices, old_num_video_input_devices);
  num_removed_input_devices =
      old_num_audio_input_devices - num_audio_input_devices +
      old_num_video_input_devices - num_video_input_devices;
  EXPECT_CALL(media_devices_manager_client_, StopRemovedInputDevice(_, _))
      .Times(num_removed_input_devices);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(2);
  audio_manager_->SetNumAudioInputDevices(num_audio_input_devices);
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      num_video_input_devices);
  audio_manager_->SetNumAudioOutputDevices(num_audio_output_devices);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  EXPECT_EQ(num_audio_input_devices, notification_all_audio_input.size());
  EXPECT_EQ(num_video_input_devices, notification_all_video_input.size());
  EXPECT_EQ(num_audio_output_devices, notification_all_audio_output.size());
  VerifyDeviceAndGroupID({notification_all_audio_input,
                          notification_all_video_input,
                          notification_all_audio_output});
}

TEST_F(MediaDevicesManagerTest, EnumerateDevicesWithCapabilities) {
  // Audio is enumerated due to heuristics to compute video group IDs.
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_));
  EXPECT_CALL(media_devices_manager_client_,
              InputDevicesChangedUI(MediaDeviceType::kMediaAudioInput, _));
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo());
  EXPECT_CALL(media_devices_manager_client_,
              InputDevicesChangedUI(MediaDeviceType::kMediaVideoInput, _));
  // Configure fake devices with video formats different from the fallback
  // formats to make sure that expected capabilities are what devices actually
  // report.
  media::FakeVideoCaptureDeviceSettings fake_device1;
  fake_device1.device_id = "fake_id_1";
  fake_device1.delivery_mode =
      media::FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS;
  fake_device1.supported_formats = {
      {{1000, 1000}, 60.0, media::PIXEL_FORMAT_I420},
      {{2000, 2000}, 120.0, media::PIXEL_FORMAT_I420}};
  fake_device1.availability = media::CameraAvailability::kAvailable;

  media::FakeVideoCaptureDeviceSettings fake_device2;
  fake_device2.device_id = "fake_id_2";
  fake_device2.delivery_mode =
      media::FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS;
  fake_device2.supported_formats = {
      {{100, 100}, 6.0, media::PIXEL_FORMAT_I420},
      {{200, 200}, 12.0, media::PIXEL_FORMAT_I420}};

  std::vector<media::FakeVideoCaptureDeviceSettings>
      fake_capture_device_settings = {fake_device1, fake_device2};
  video_capture_device_factory_->SetToCustomDevicesConfig(
      fake_capture_device_settings);

  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] =
      true;

  InitializeRenderFrameHost();

  base::RunLoop run_loop;
  media_devices_manager_->EnumerateAndRankDevices(
      {-1, -1}, devices_to_enumerate, true, true,
      base::BindOnce(
          &MediaDevicesManagerTest::EnumerateWithCapabilitiesCallback,
          base::Unretained(this), fake_capture_device_settings, &run_loop));
  run_loop.Run();
}

TEST_F(MediaDevicesManagerTest, EnumerateDevicesUnplugDefaultDevice) {
  // This tests does not apply to CrOS, which is to seamlessly switch device.
#if !BUILDFLAG(IS_CHROMEOS)
  std::string default_device_id("fake_device_id_1");
  std::string new_default_device_id("fake_device_id_2");

  EXPECT_EQ(removed_device_ids_.size(), 0u);

  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(2);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(2);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(2);
  // Since we will be removing the default device, we expect that we will remove
  // both the actual device as well as the old instance of the device with
  // 'default' ID.
  blink::WebMediaDeviceInfo removed_devices;
  EXPECT_CALL(media_devices_manager_client_, StopRemovedInputDevice(_, _))
      .Times(2)
      .WillRepeatedly(
          Invoke(this, &MediaDevicesManagerTest::TrackRemovedDevice));

  // Setup the configuration and run the devices.
  audio_manager_->SetDefaultDeviceToId(default_device_id);
  RunEnumerateDevices();

  // Unplug the default device and switch default to a different device.
  audio_manager_->RemoveInputAudioDeviceById(default_device_id);
  audio_manager_->SetDefaultDeviceToId(new_default_device_id);
  RunEnumerateDevices();

  EXPECT_EQ(removed_device_ids_.size(), 2u);
  EXPECT_TRUE(base::Contains(removed_device_ids_, default_device_id));
  EXPECT_TRUE(base::Contains(removed_device_ids_,
                             media::AudioDeviceDescription::kDefaultDeviceId));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

TEST_F(MediaDevicesManagerTest, EnumerateDevicesUnplugCommunicationsDevice) {
  // This test has only significance on Windows devices, since communication
  // devices can only be found on windows.
#if BUILDFLAG(IS_WIN)
  std::string communications_device_id("fake_device_id_1");
  std::string new_communications_device_id("fake_device_id_2");

  EXPECT_EQ(removed_device_ids_.size(), 0u);

  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(2);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(2);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(2);
  // Since we will be removing the communications device, we expect that we will
  // remove both the actual device as well as the old instance of the device
  // with 'communications' ID.
  blink::WebMediaDeviceInfo removed_devices;
  EXPECT_CALL(media_devices_manager_client_, StopRemovedInputDevice(_, _))
      .Times(2)
      .WillRepeatedly(
          Invoke(this, &MediaDevicesManagerTest::TrackRemovedDevice));

  // Setup the configuration and run the devices.
  audio_manager_->SetCommunicationsDeviceToId(communications_device_id);
  RunEnumerateDevices();

  // Unplug the default device and switch default to a different device.
  audio_manager_->RemoveInputAudioDeviceById(communications_device_id);
  audio_manager_->SetCommunicationsDeviceToId(new_communications_device_id);
  RunEnumerateDevices();

  EXPECT_EQ(removed_device_ids_.size(), 2u);
  EXPECT_TRUE(base::Contains(removed_device_ids_, communications_device_id));
  EXPECT_TRUE(
      base::Contains(removed_device_ids_,
                     media::AudioDeviceDescription::kCommunicationsDeviceId));
#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(MediaDevicesManagerTest,
       EnumerateDevicesUnplugDefaultAndCommunicationsDevice) {
  // This test has only significance on Windows devices, since communication
  // devices can only be found on windows.
#if BUILDFLAG(IS_WIN)
  // The two device IDs that will be used as 'default' and 'communications'
  // devices.
  std::string target_device_id("fake_device_id_1");
  std::string new_target_device_id("fake_device_id_2");

  EXPECT_EQ(removed_device_ids_.size(), 0u);

  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(2);
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(2);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(2);
  // Since we will be removing the device identified as 'default' and
  // 'communications, we expect that we will
  // remove the actual device as well as the old instances of the device
  // with 'communications' and 'default' IDs.
  blink::WebMediaDeviceInfo removed_devices;
  EXPECT_CALL(media_devices_manager_client_, StopRemovedInputDevice(_, _))
      .Times(3)
      .WillRepeatedly(
          Invoke(this, &MediaDevicesManagerTest::TrackRemovedDevice));

  // Setup the configuration and run the devices.
  audio_manager_->SetDefaultDeviceToId(target_device_id);
  audio_manager_->SetCommunicationsDeviceToId(target_device_id);
  RunEnumerateDevices();

  // Unplug the default device and switch default to a different device.
  audio_manager_->RemoveInputAudioDeviceById(target_device_id);
  audio_manager_->SetDefaultDeviceToId(new_target_device_id);
  audio_manager_->SetCommunicationsDeviceToId(new_target_device_id);
  RunEnumerateDevices();

  EXPECT_EQ(removed_device_ids_.size(), 3u);
  EXPECT_TRUE(base::Contains(removed_device_ids_, target_device_id));
  EXPECT_TRUE(base::Contains(removed_device_ids_,
                             media::AudioDeviceDescription::kDefaultDeviceId));
  EXPECT_TRUE(
      base::Contains(removed_device_ids_,
                     media::AudioDeviceDescription::kCommunicationsDeviceId));
#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(MediaDevicesManagerTest, GuessVideoGroupID) {
  blink::WebMediaDeviceInfoArray audio_devices = {
      {media::AudioDeviceDescription::kDefaultDeviceId,
       "Default - Unique USB Mic (1234:5678)", "group_1"},
      {media::AudioDeviceDescription::kCommunicationsDeviceId,
       "communications - Unique USB Mic (1234:5678)", "group_1"},
      {"device_id_1", "Microphone (Logitech Webcam C930e)", "group_1"},
      {"device_id_2", "HD Pro Webcam C920", "group_2"},
      {"device_id_3", "Microsoft® LifeCam Cinema(TM)", "group_3"},
      {"device_id_4", "Repeated webcam", "group_4"},
      {"device_id_5", "Repeated webcam", "group_5"},
      {"device_id_6", "Dual-mic webcam device", "group_6"},
      {"device_id_7", "Dual-mic webcam device", "group_6"},
      {"device_id_8", "Repeated dual-mic webcam device", "group_7"},
      {"device_id_9", "Repeated dual-mic webcam device", "group_7"},
      {"device_id_10", "Repeated dual-mic webcam device", "group_8"},
      {"device_id_11", "Repeated dual-mic webcam device", "group_8"},
      {"device_id_12", "Unique USB Mic (1234:5678)", "group_1"},
      {"device_id_13", "Another Unique USB Mic (5678:9abc)", "group_9"},
      {"device_id_14", "Repeated USB Mic (8765:4321)", "group_10"},
      {"device_id_15", "Repeated USB Mic (8765:4321)", "group_11"},
      // Extra entry for communications device added here just to make sure
      // that it is not incorrectly detected as an extra real device. Real
      // enumerations contain only the first entry for the communications
      // device.
      {media::AudioDeviceDescription::kCommunicationsDeviceId,
       "communications - Unique USB Mic (1234:5678)", "group_1"},
  };

  blink::WebMediaDeviceInfo logitech_video(
      "logitech_video", "Logitech Webcam C930e (046d:0843)", "");
  blink::WebMediaDeviceInfo hd_pro_video("hd_pro_video",
                                         "HD Pro Webcam C920 (046d:082d)", "");
  blink::WebMediaDeviceInfo lifecam_video(
      "lifecam_video", "Microsoft® LifeCam Cinema(TM) (045e:075d)", "");
  blink::WebMediaDeviceInfo repeated_webcam1_video("repeated_webcam_1",
                                                   "Repeated webcam", "");
  blink::WebMediaDeviceInfo repeated_webcam2_video("repeated_webcam_2",
                                                   "Repeated webcam", "");
  blink::WebMediaDeviceInfo dual_mic_video(
      "dual_mic_video", "Dual-mic webcam device (1111:1111)", "");
  blink::WebMediaDeviceInfo webcam_only_video("webcam_only_video",
                                              "Webcam-only device", "");
  blink::WebMediaDeviceInfo repeated_dual_mic1_video(
      "repeated_dual_mic1_video", "Repeated dual-mic webcam device", "");
  blink::WebMediaDeviceInfo repeated_dual_mic2_video(
      "repeated_dual_mic2_video", "Repeated dual-mic webcam device", "");
  blink::WebMediaDeviceInfo short_label_video("short_label_video", " ()", "");
  blink::WebMediaDeviceInfo unique_usb_video(
      "unique_usb_video", "Unique USB webcam (1234:5678)", "");
  blink::WebMediaDeviceInfo another_unique_usb_video(
      "another_unique_usb_video", "Another Unique USB webcam (5678:9abc)", "");
  blink::WebMediaDeviceInfo repeated_usb1_video(
      "repeated_usb1_video", "Repeated USB webcam (8765:4321)", "");
  blink::WebMediaDeviceInfo repeated_usb2_video(
      "repeated_usb2_video", "Repeated USB webcam (8765:4321)", "");

  EXPECT_EQ(GuessVideoGroupID(audio_devices, logitech_video), "group_1");
  EXPECT_EQ(GuessVideoGroupID(audio_devices, hd_pro_video), "group_2");
  EXPECT_EQ(GuessVideoGroupID(audio_devices, lifecam_video), "group_3");
  EXPECT_EQ(GuessVideoGroupID(audio_devices, repeated_webcam1_video),
            repeated_webcam1_video.device_id);
  EXPECT_EQ(GuessVideoGroupID(audio_devices, repeated_webcam2_video),
            repeated_webcam2_video.device_id);
  EXPECT_EQ(GuessVideoGroupID(audio_devices, dual_mic_video), "group_6");
  EXPECT_EQ(GuessVideoGroupID(audio_devices, webcam_only_video),
            webcam_only_video.device_id);
  EXPECT_EQ(GuessVideoGroupID(audio_devices, repeated_dual_mic1_video),
            repeated_dual_mic1_video.device_id);
  EXPECT_EQ(GuessVideoGroupID(audio_devices, repeated_dual_mic2_video),
            repeated_dual_mic2_video.device_id);
  EXPECT_EQ(GuessVideoGroupID(audio_devices, short_label_video),
            short_label_video.device_id);
  EXPECT_EQ(GuessVideoGroupID(audio_devices, unique_usb_video), "group_1");
  EXPECT_EQ(GuessVideoGroupID(audio_devices, another_unique_usb_video),
            "group_9");
  EXPECT_EQ(GuessVideoGroupID(audio_devices, repeated_usb1_video),
            repeated_usb1_video.device_id);
  EXPECT_EQ(GuessVideoGroupID(audio_devices, repeated_usb2_video),
            repeated_usb2_video.device_id);
}

TEST_F(MediaDevicesManagerTest, DeviceIdSaltReset) {
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo()).Times(2);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _))
      .Times(1);

  size_t num_video_input_devices = 4;
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      num_video_input_devices);

  // Run an enumeration to make sure |media_devices_manager_| has the new
  // configuration.
  EXPECT_CALL(*this, MockCallback(_));
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;
  base::RunLoop run_loop;
  media_devices_manager_->EnumerateDevices(
      devices_to_enumerate,
      base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();

  InitializeRenderFrameHost();

  // Add device-change event listener.
  MockMediaDevicesListener listener_video_input;
  MediaDevicesManager::BoolDeviceTypes video_input_devices_to_subscribe;
  video_input_devices_to_subscribe[static_cast<size_t>(
      MediaDeviceType::kMediaVideoInput)] = true;
  media_devices_manager_->SubscribeDeviceChangeNotifications(
      kRenderFrameHostId, video_input_devices_to_subscribe,
      listener_video_input.CreateInterfacePtrAndBind());

  // Expect an OnDevicesChanged event.
  blink::WebMediaDeviceInfoArray notification_video_input;
  EXPECT_CALL(listener_video_input,
              OnDevicesChanged(MediaDeviceType::kMediaVideoInput, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&notification_video_input));

  base::RunLoop().RunUntilIdle();

  // Set a new salt and notify MDM.
  salt = "new-device-id-salt";
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  EXPECT_EQ(num_video_input_devices, notification_video_input.size());
}

TEST_F(MediaDevicesManagerTest, EnumerateVideoInputFailsOnce) {
  // Inject an UnknownError on the first call to GetDeviceInfosAsync, otherwise
  // fall through to the video_capture_system_.
  EXPECT_CALL(*mock_video_capture_provider_, GetDeviceInfosAsync(_))
      .Times(kNumCalls)
      .WillOnce(Invoke(
          [&](VideoCaptureProvider::GetDeviceInfosCallback result_callback) {
            std::move(result_callback)
                .Run(DeviceEnumerationResult::kUnknownError, {});
          }))
      .WillRepeatedly(Invoke(
          [&](VideoCaptureProvider::GetDeviceInfosCallback result_callback) {
            video_capture_system_->GetDeviceInfosAsync(base::BindOnce(
                std::move(result_callback),
                media::mojom::DeviceEnumerationResult::kSuccess));
          }));
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo())
      .Times(kNumCalls - 1);

  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*this, MockCallback(_)).Times(kNumCalls);
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;
  for (int i = 0; i < kNumCalls; i++) {
    base::RunLoop run_loop;
    media_devices_manager_->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                       base::Unretained(this), &run_loop));
    run_loop.Run();
  }
  ExpectVideoEnumerationHistogramReport(/*success_count=*/kNumCalls - 1,
                                        /*error_count=*/1);
}

TEST_F(MediaDevicesManagerTest, RegisterUnregisterDispatcherHosts) {
  mojo::Remote<blink::mojom::MediaDevicesDispatcherHost> client1;
  media_devices_manager_->RegisterDispatcherHost(
      std::make_unique<MockMediaDevicesDispatcherHost>(),
      client1.BindNewPipeAndPassReceiver());
  EXPECT_EQ(media_devices_manager_->num_registered_dispatcher_hosts(), 1u);

  mojo::Remote<blink::mojom::MediaDevicesDispatcherHost> client2;
  media_devices_manager_->RegisterDispatcherHost(
      std::make_unique<MockMediaDevicesDispatcherHost>(),
      client2.BindNewPipeAndPassReceiver());
  EXPECT_EQ(media_devices_manager_->num_registered_dispatcher_hosts(), 2u);

  // Closing a pipe deregisters the corresponding dispatcher host.
  client1.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(media_devices_manager_->num_registered_dispatcher_hosts(), 1u);

  // Destroying the MediaDevicesManager disconnects the remaining pipe.
  EXPECT_TRUE(client2.is_connected());
  media_devices_manager_ = nullptr;
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(client2.is_connected());
}

TEST_F(MediaDevicesManagerTest, DevicePropertyChanges) {
  EXPECT_CALL(*audio_manager_, MockGetAudioOutputDeviceNames(_)).Times(0);
  EXPECT_CALL(*audio_manager_, MockGetAudioInputDeviceNames(_)).Times(0);
  EXPECT_CALL(media_devices_manager_client_, StopRemovedInputDevice(_, _));
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));

  std::vector<media::FakeVideoCaptureDeviceSettings> device_config(1);
  device_config[0].device_id = "device_id";
  device_config[0].availability = media::CameraAvailability::kAvailable;

  video_capture_device_factory_->SetToCustomDevicesConfig(device_config);

  // Run an enumeration to make sure |media_devices_manager_| has the new
  // configuration.
  MediaDeviceEnumeration enumeration;
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo());
  EXPECT_CALL(*this, MockCallback(_)).WillRepeatedly(SaveArg<0>(&enumeration));

  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] =
      true;
  base::RunLoop run_loop;
  media_devices_manager_->EnumerateDevices(
      devices_to_enumerate,
      base::BindOnce(&MediaDevicesManagerTest::EnumerateCallback,
                     base::Unretained(this), &run_loop));
  run_loop.Run();
  blink::WebMediaDeviceInfoArray& video_devices =
      enumeration[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)];
  EXPECT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].device_id, device_config[0].device_id);
  EXPECT_EQ(video_devices[0].availability, device_config[0].availability);

  // Add device-change event listener.
  MockMediaDevicesListener listener;
  MediaDevicesManager::BoolDeviceTypes video_input_devices_to_subscribe;
  video_input_devices_to_subscribe[static_cast<size_t>(
      MediaDeviceType::kMediaVideoInput)] = true;
  media_devices_manager_->SubscribeDeviceChangeNotifications(
      kRenderFrameHostId, video_input_devices_to_subscribe,
      listener.CreateInterfacePtrAndBind());

  // Change device availability
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo());
  blink::WebMediaDeviceInfoArray notification_video_input;
  EXPECT_CALL(listener, OnDevicesChanged(MediaDeviceType::kMediaVideoInput, _))
      .WillOnce(SaveArg<1>(&notification_video_input));

  device_config[0].availability =
      media::CameraAvailability::kUnavailableExclusivelyUsedByOtherApplication;
  video_capture_device_factory_->SetToCustomDevicesConfig(device_config);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  EXPECT_EQ(notification_video_input.size(), device_config.size());
  EXPECT_EQ(notification_video_input[0].availability,
            device_config[0].availability);

  // Change device ID
  EXPECT_CALL(media_devices_manager_client_, InputDevicesChangedUI(_, _));
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo());
  EXPECT_CALL(listener, OnDevicesChanged(MediaDeviceType::kMediaVideoInput, _))
      .WillOnce(SaveArg<1>(&notification_video_input));

  device_config[0].device_id = "new_device_id";
  video_capture_device_factory_->SetToCustomDevicesConfig(device_config);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  EXPECT_EQ(notification_video_input.size(), 1u);
  // Not expecting equality between the configured device ID and the one in the
  // notification because the notification's device ID is hashed.

  // Change supported formats. Expect no notification.
  EXPECT_CALL(*video_capture_device_factory_, MockGetDevicesInfo());
  EXPECT_CALL(listener, OnDevicesChanged(_, _)).Times(0);
  device_config[0].supported_formats.emplace_back(gfx::Size(100, 100), 10.0f,
                                                  media::PIXEL_FORMAT_I420);
  video_capture_device_factory_->SetToCustomDevicesConfig(device_config);
  FireDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
}

}  // namespace content
