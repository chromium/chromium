// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/system/system_monitor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/mock_video_capture_provider.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#endif

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace content {

namespace {

constexpr int kProcessId = 5;
constexpr int kRenderId = 6;
constexpr int kRequesterId = 7;
constexpr int kPageRequestId = 8;
constexpr const char* kRegularVideoDeviceId = "stub_device_0";
constexpr const char* kDepthVideoDeviceId = "stub_device_1 (depth)";
constexpr media::VideoCaptureApi kStubCaptureApi =
    media::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;

void AudioInputDevicesEnumerated(base::Closure quit_closure,
                                 media::AudioDeviceDescriptions* out,
                                 const MediaDeviceEnumeration& enumeration) {
  for (const auto& info : enumeration[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT]) {
    out->emplace_back(info.label, info.device_id, info.group_id);
  }
  std::move(quit_closure).Run();
}

}  // anonymous namespace

class MockMediaStreamDispatcherHost
    : public MediaStreamDispatcherHost,
      public blink::mojom::MediaStreamDeviceObserver {
 public:
  MockMediaStreamDispatcherHost(int render_process_id,
                                int render_frame_id,
                                MediaStreamManager* manager)
      : MediaStreamDispatcherHost(render_process_id, render_frame_id, manager),
        task_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  ~MockMediaStreamDispatcherHost() override {}

  // A list of mock methods.
  MOCK_METHOD3(OnStreamGenerationSuccess,
               void(int request_id,
                    int audio_array_size,
                    int video_array_size));
  MOCK_METHOD2(OnStreamGenerationFailure,
               void(int request_id,
                    blink::mojom::MediaStreamRequestResult result));
  MOCK_METHOD0(OnDeviceStopSuccess, void());
  MOCK_METHOD0(OnDeviceOpenSuccess, void());

  // Accessor to private functions.
  void OnGenerateStream(int page_request_id,
                        const blink::StreamControls& controls,
                        const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::GenerateStream(
        page_request_id, controls, false,
        blink::mojom::StreamSelectionInfo::New(
            blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
            base::nullopt),
        base::BindOnce(&MockMediaStreamDispatcherHost::OnStreamGenerated,
                       base::Unretained(this), page_request_id));
  }

  void OnStopStreamDevice(const std::string& device_id,
                          const base::UnguessableToken& session_id) {
    MediaStreamDispatcherHost::StopStreamDevice(device_id, session_id);
  }

  void OnOpenDevice(int page_request_id,
                    const std::string& device_id,
                    blink::mojom::MediaStreamType type,
                    const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::OpenDevice(
        page_request_id, device_id, type,
        base::BindOnce(&MockMediaStreamDispatcherHost::OnDeviceOpened,
                       base::Unretained(this)));
  }

  void OnStreamStarted(const std::string& label) override {
    MediaStreamDispatcherHost::OnStreamStarted(label);
  }

  // mojom::MediaStreamDeviceObserver implementation.
  void OnDeviceStopped(const std::string& label,
                       const blink::MediaStreamDevice& device) override {
    OnDeviceStoppedInternal(label, device);
  }

  // mojom::MediaStreamDeviceObserver implementation.
  void OnDeviceChanged(const std::string& label,
                       const blink::MediaStreamDevice& old_device,
                       const blink::MediaStreamDevice& new_device) override {}

  mojo::PendingRemote<blink::mojom::MediaStreamDeviceObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  std::string label_;
  blink::MediaStreamDevices audio_devices_;
  blink::MediaStreamDevices video_devices_;
  blink::MediaStreamDevice opened_device_;

 private:
  // These handler methods do minimal things and delegate to the mock methods.
  void OnStreamGenerated(int request_id,
                         blink::mojom::MediaStreamRequestResult result,
                         const std::string& label,
                         const blink::MediaStreamDevices& audio_devices,
                         const blink::MediaStreamDevices& video_devices) {
    if (result != blink::mojom::MediaStreamRequestResult::OK) {
      OnStreamGenerationFailed(request_id, result);
      return;
    }

    OnStreamGenerationSuccess(request_id, audio_devices.size(),
                              video_devices.size());
    // Simulate the stream started event back to host for UI testing.
    OnStreamStarted(label);

    // Notify that the event have occurred.
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    task_runner_->PostTask(FROM_HERE, std::move(quit_closure));

    label_ = label;
    audio_devices_ = audio_devices;
    video_devices_ = video_devices;
  }

  void OnStreamGenerationFailed(int request_id,
                                blink::mojom::MediaStreamRequestResult result) {
    OnStreamGenerationFailure(request_id, result);
    if (!quit_closures_.empty()) {
      base::Closure quit_closure = quit_closures_.front();
      quit_closures_.pop();
      task_runner_->PostTask(FROM_HERE, std::move(quit_closure));
    }

    label_.clear();
  }

  void OnDeviceStoppedInternal(const std::string& label,
                               const blink::MediaStreamDevice& device) {
    if (blink::IsVideoInputMediaType(device.type))
      EXPECT_TRUE(device.IsSameDevice(video_devices_[0]));
    if (blink::IsAudioInputMediaType(device.type))
      EXPECT_TRUE(device.IsSameDevice(audio_devices_[0]));

    OnDeviceStopSuccess();
  }

  void OnDeviceOpened(bool success,
                      const std::string& label,
                      const blink::MediaStreamDevice& device) {
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    task_runner_->PostTask(FROM_HERE, std::move(quit_closure));
    if (success) {
      label_ = label;
      opened_device_ = device;
      OnDeviceOpenSuccess();
    }
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::queue<base::Closure> quit_closures_;
  mojo::Receiver<blink::mojom::MediaStreamDeviceObserver> receiver_{this};
};

class MockMediaStreamUIProxy : public FakeMediaStreamUIProxy {
 public:
  MockMediaStreamUIProxy()
      : FakeMediaStreamUIProxy(/*tests_use_fake_render_frame_hosts=*/true) {}
  void OnStarted(
      base::OnceClosure stop,
      content::MediaStreamUI::SourceCallback source,
      MediaStreamUIProxy::WindowIdCallback window_id_callback) override {
    // gmock cannot handle move-only types:
    MockOnStarted(base::AdaptCallbackForRepeating(std::move(stop)));
  }

  MOCK_METHOD1(MockOnStarted, void(base::Closure stop));
};

class MediaStreamDispatcherHostTest : public testing::Test {
 public:
  MediaStreamDispatcherHostTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        origin_(url::Origin::Create(GURL("https://test.com"))) {
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    browser_context_ = std::make_unique<TestBrowserContext>();
    // Make sure we use fake devices to avoid long delays.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    auto mock_video_capture_provider =
        std::make_unique<MockVideoCaptureProvider>();
    mock_video_capture_provider_ = mock_video_capture_provider.get();
    // Create our own MediaStreamManager.
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), audio_manager_->GetTaskRunner(),
        std::move(mock_video_capture_provider));

    host_ = std::make_unique<MockMediaStreamDispatcherHost>(
        kProcessId, kRenderId, media_stream_manager_.get());
    host_->set_salt_and_origin_callback_for_testing(
        base::BindRepeating(&MediaStreamDispatcherHostTest::GetSaltAndOrigin,
                            base::Unretained(this)));
    host_->SetMediaStreamDeviceObserverForTesting(
        host_->BindNewPipeAndPassRemote());

#if defined(OS_CHROMEOS)
    chromeos::CrasAudioClient::InitializeFake();
    chromeos::CrasAudioHandler::InitializeForTesting();
#endif
  }

  ~MediaStreamDispatcherHostTest() override {
    audio_manager_->Shutdown();
#if defined(OS_CHROMEOS)
    chromeos::CrasAudioHandler::Shutdown();
    chromeos::CrasAudioClient::Shutdown();
#endif
  }

  void SetUp() override {
    stub_video_device_ids_.emplace_back(kRegularVideoDeviceId);
    stub_video_device_ids_.emplace_back(kDepthVideoDeviceId);
    ON_CALL(*mock_video_capture_provider_, DoGetDeviceInfosAsync(_))
        .WillByDefault(Invoke(
            [this](
                VideoCaptureProvider::GetDeviceInfosCallback& result_callback) {
              std::vector<media::VideoCaptureDeviceInfo> result;
              for (const auto& device_id : stub_video_device_ids_) {
                media::VideoCaptureDeviceInfo info;
                info.descriptor.device_id = device_id;
                info.descriptor.capture_api = kStubCaptureApi;
                result.push_back(info);
              }
              std::move(result_callback).Run(result);
            }));

    base::RunLoop run_loop;
    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] = true;
    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&AudioInputDevicesEnumerated, run_loop.QuitClosure(),
                       &audio_device_descriptions_));
    run_loop.Run();

    ASSERT_GT(audio_device_descriptions_.size(), 0u);
  }

  void TearDown() override { host_.reset(); }

  MediaDeviceSaltAndOrigin GetSaltAndOrigin(int /* process_id */,
                                            int /* frame_id */) {
    return MediaDeviceSaltAndOrigin(browser_context_->GetMediaDeviceIDSalt(),
                                    "fake_group_id_salt", origin_);
  }

 protected:
  std::unique_ptr<FakeMediaStreamUIProxy> CreateMockUI(bool expect_started) {
    std::unique_ptr<MockMediaStreamUIProxy> fake_ui =
        std::make_unique<MockMediaStreamUIProxy>();

    if (expect_started)
      EXPECT_CALL(*fake_ui, MockOnStarted(_));
    return fake_ui;
  }

  virtual void SetupFakeUI(bool expect_started) {
    media_stream_manager_->UseFakeUIFactoryForTests(
        base::Bind(&MediaStreamDispatcherHostTest::CreateMockUI,
                   base::Unretained(this), expect_started));
  }

  void GenerateStreamAndWaitForResult(int page_request_id,
                                      const blink::StreamControls& controls) {
    base::RunLoop run_loop;
    int expected_audio_array_size =
        (controls.audio.requested && !audio_device_descriptions_.empty()) ? 1
                                                                          : 0;
    int expected_video_array_size =
        (controls.video.requested && !stub_video_device_ids_.empty()) ? 1 : 0;
    EXPECT_CALL(*host_, OnStreamGenerationSuccess(page_request_id,
                                                  expected_audio_array_size,
                                                  expected_video_array_size));
    host_->OnGenerateStream(page_request_id, controls, run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->audio_devices_));
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->audio_devices_, origin_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  void GenerateStreamAndWaitForFailure(
      int page_request_id,
      const blink::StreamControls& controls,
      blink::mojom::MediaStreamRequestResult expected_result) {
    base::RunLoop run_loop;
    EXPECT_CALL(*host_,
                OnStreamGenerationFailure(page_request_id, expected_result));
    host_->OnGenerateStream(page_request_id, controls, run_loop.QuitClosure());
    run_loop.Run();
  }

  void OpenVideoDeviceAndWaitForResult(int page_request_id,
                                       const std::string& device_id) {
    EXPECT_CALL(*host_, OnDeviceOpenSuccess());
    base::RunLoop run_loop;
    host_->OnOpenDevice(page_request_id, device_id,
                        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                        run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  void OpenVideoDeviceAndWaitForFailure(int page_request_id,
                                        const std::string& device_id) {
    EXPECT_CALL(*host_, OnDeviceOpenSuccess()).Times(0);
    base::RunLoop run_loop;
    host_->OnOpenDevice(page_request_id, device_id,
                        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                        run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_FALSE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  bool DoesContainRawIds(const blink::MediaStreamDevices& devices) {
    for (size_t i = 0; i < devices.size(); ++i) {
      if (devices[i].id != media::AudioDeviceDescription::kDefaultDeviceId &&
          devices[i].id !=
              media::AudioDeviceDescription::kCommunicationsDeviceId) {
        for (const auto& audio_device : audio_device_descriptions_) {
          if (audio_device.unique_id == devices[i].id)
            return true;
        }
      }
      for (const std::string& device_id : stub_video_device_ids_) {
        if (device_id == devices[i].id)
          return true;
      }
    }
    return false;
  }

  bool DoesEveryDeviceMapToRawId(const blink::MediaStreamDevices& devices,
                                 const url::Origin& origin) {
    for (size_t i = 0; i < devices.size(); ++i) {
      bool found_match = false;
      media::AudioDeviceDescriptions::const_iterator audio_it =
          audio_device_descriptions_.begin();
      for (; audio_it != audio_device_descriptions_.end(); ++audio_it) {
        if (DoesMediaDeviceIDMatchHMAC(browser_context_->GetMediaDeviceIDSalt(),
                                       origin, devices[i].id,
                                       audio_it->unique_id)) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      for (const std::string& device_id : stub_video_device_ids_) {
        if (DoesMediaDeviceIDMatchHMAC(browser_context_->GetMediaDeviceIDSalt(),
                                       origin, devices[i].id, device_id)) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      if (!found_match)
        return false;
    }
    return true;
  }

  std::unique_ptr<MockMediaStreamDispatcherHost> host_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  media::AudioDeviceDescriptions audio_device_descriptions_;
  std::vector<std::string> stub_video_device_ids_;
  url::Origin origin_;
  MockVideoCaptureProvider* mock_video_capture_provider_;
};

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithVideoOnly) {
  blink::StreamControls controls(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioOnly) {
  blink::StreamControls controls(true, false);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 0u);
}

// This test simulates a shutdown scenario: we don't setup a fake UI proxy for
// MediaStreamManager, so it will create an ordinary one which will not find
// a RenderFrameHostDelegate. This normally should only be the case at shutdown.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithNothing) {
  blink::StreamControls controls(false, false);

  GenerateStreamAndWaitForFailure(
      kPageRequestId, controls,
      blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioAndVideo) {
  blink::StreamControls controls(true, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithDepthVideo) {
  // We specify to generate both audio and video stream.
  blink::StreamControls controls(true, true);
  std::string source_id = GetHMACForMediaDeviceID(
      browser_context_->GetMediaDeviceIDSalt(), origin_, kDepthVideoDeviceId);
  // |source_id| corresponds to the depth device. As we can generate only one
  // video stream using GenerateStreamAndWaitForResult, we use
  // controls.video.source_id to specify that the stream is depth video.
  // See also MediaStreamManager::GenerateStream and other tests here.
  controls.video.device_id = source_id;

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  // We specified the generation and expect to get
  // one audio and one depth video stream.
  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

// This test generates two streams with video only using the same render frame
// id. The same capture device with the same device and session id is expected
// to be used.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsFromSameRenderId) {
  blink::StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().id;
  const base::UnguessableToken session_id1 =
      host_->video_devices_.front().session_id();

  // Generate second stream.
  GenerateStreamAndWaitForResult(kPageRequestId + 1, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label2 = host_->label_;
  const std::string device_id2 = host_->video_devices_.front().id;
  const base::UnguessableToken session_id2 =
      host_->video_devices_.front().session_id();
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_EQ(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamAndOpenDeviceFromSameRenderFrame) {
  SetupFakeUI(true);
  blink::StreamControls controls(false, true);

  // Generate first stream.
  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().id;
  const base::UnguessableToken session_id1 =
      host_->video_devices_.front().session_id();

  // Generate second stream.
  OpenVideoDeviceAndWaitForResult(kPageRequestId, device_id1);

  const std::string device_id2 = host_->opened_device_.id;
  const base::UnguessableToken session_id2 = host_->opened_device_.session_id();
  const std::string label2 = host_->label_;

  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

// This test generates two streams with video only using two separate render
// frame ids. The same device id but different session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsDifferentRenderId) {
  blink::StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().id;
  const base::UnguessableToken session_id1 =
      host_->video_devices_.front().session_id();

  // Generate second stream from another render frame.
  host_ = std::make_unique<MockMediaStreamDispatcherHost>(
      kProcessId, kRenderId + 1, media_stream_manager_.get());
  host_->set_salt_and_origin_callback_for_testing(
      base::BindRepeating(&MediaStreamDispatcherHostTest::GetSaltAndOrigin,
                          base::Unretained(this)));
  host_->SetMediaStreamDeviceObserverForTesting(
      host_->BindNewPipeAndPassRemote());

  GenerateStreamAndWaitForResult(kPageRequestId + 1, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label2 = host_->label_;
  const std::string device_id2 = host_->video_devices_.front().id;
  const base::UnguessableToken session_id2 =
      host_->video_devices_.front().session_id();
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

// This test request two streams with video only without waiting for the first
// stream to be generated before requesting the second.
// The same device id and session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithoutWaiting) {
  blink::StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  {
    InSequence s;
    EXPECT_CALL(*host_, OnStreamGenerationSuccess(kPageRequestId, 0, 1));

    // Generate second stream.
    EXPECT_CALL(*host_, OnStreamGenerationSuccess(kPageRequestId + 1, 0, 1));
  }
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  host_->OnGenerateStream(kPageRequestId, controls, run_loop1.QuitClosure());
  host_->OnGenerateStream(kPageRequestId + 1, controls,
                          run_loop2.QuitClosure());

  run_loop1.Run();
  run_loop2.Run();
}

// Test that we can generate streams where a sourceId is specified in
// the request.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithSourceId) {
  ASSERT_GE(audio_device_descriptions_.size(), 1u);
  ASSERT_GE(stub_video_device_ids_.size(), 1u);

  media::AudioDeviceDescriptions::const_iterator audio_it =
      audio_device_descriptions_.begin();
  for (; audio_it != audio_device_descriptions_.end(); ++audio_it) {
    std::string source_id = GetHMACForMediaDeviceID(
        browser_context_->GetMediaDeviceIDSalt(), origin_, audio_it->unique_id);
    ASSERT_FALSE(source_id.empty());
    blink::StreamControls controls(true, true);
    controls.audio.device_id = source_id;

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kPageRequestId, controls);
    EXPECT_EQ(host_->audio_devices_[0].id, source_id);
  }

  for (const std::string& device_id : stub_video_device_ids_) {
    std::string source_id = GetHMACForMediaDeviceID(
        browser_context_->GetMediaDeviceIDSalt(), origin_, device_id);
    ASSERT_FALSE(source_id.empty());
    blink::StreamControls controls(true, true);
    controls.video.device_id = source_id;

    GenerateStreamAndWaitForResult(kPageRequestId, controls);
    EXPECT_EQ(host_->video_devices_[0].id, source_id);
  }
}

// Test that generating a stream with an invalid video source id fail.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithInvalidVideoSourceId) {
  blink::StreamControls controls(true, true);
  controls.video.device_id = "invalid source id";

  GenerateStreamAndWaitForFailure(
      kPageRequestId, controls,
      blink::mojom::MediaStreamRequestResult::NO_HARDWARE);
}

// Test that generating a stream with an invalid audio source id fail.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithInvalidAudioSourceId) {
  blink::StreamControls controls(true, true);
  controls.audio.device_id = "invalid source id";

  GenerateStreamAndWaitForFailure(
      kPageRequestId, controls,
      blink::mojom::MediaStreamRequestResult::NO_HARDWARE);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsNoAvailableVideoDevice) {
  stub_video_device_ids_.clear();
  blink::StreamControls controls(true, true);

  SetupFakeUI(false);
  GenerateStreamAndWaitForFailure(
      kPageRequestId, controls,
      blink::mojom::MediaStreamRequestResult::NO_HARDWARE);
}

// Test that if a OnStopStreamDevice message is received for a device that has
// been opened in a MediaStream and by pepper, the device is only stopped for
// the MediaStream.
TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStream) {
  blink::StreamControls controls(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  std::string stream_request_label = host_->label_;
  blink::MediaStreamDevice video_device = host_->video_devices_.front();
  ASSERT_EQ(
      1u, media_stream_manager_->GetDevicesOpenedByRequest(stream_request_label)
              .size());

  // Open the same device by Pepper.
  OpenVideoDeviceAndWaitForResult(kPageRequestId, video_device.id);
  std::string open_device_request_label = host_->label_;

  // Stop the device in the MediaStream.
  host_->OnStopStreamDevice(video_device.id, video_device.session_id());

  EXPECT_EQ(
      0u, media_stream_manager_->GetDevicesOpenedByRequest(stream_request_label)
              .size());
  EXPECT_EQ(1u, media_stream_manager_
                    ->GetDevicesOpenedByRequest(open_device_request_label)
                    .size());
}

TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStreamAndRestart) {
  blink::StreamControls controls(true, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  std::string request_label1 = host_->label_;
  blink::MediaStreamDevice video_device = host_->video_devices_.front();
  // Expect that 1 audio and 1 video device has been opened.
  EXPECT_EQ(
      2u,
      media_stream_manager_->GetDevicesOpenedByRequest(request_label1).size());

  host_->OnStopStreamDevice(video_device.id, video_device.session_id());
  EXPECT_EQ(
      1u,
      media_stream_manager_->GetDevicesOpenedByRequest(request_label1).size());

  GenerateStreamAndWaitForResult(kPageRequestId, controls);
  std::string request_label2 = host_->label_;

  blink::MediaStreamDevices request1_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label1);
  blink::MediaStreamDevices request2_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label2);

  ASSERT_EQ(1u, request1_devices.size());
  ASSERT_EQ(2u, request2_devices.size());

  // Test that the same audio device has been opened in both streams.
  EXPECT_TRUE(request1_devices[0].IsSameDevice(request2_devices[0]) ||
              request1_devices[0].IsSameDevice(request2_devices[1]));
}

TEST_F(MediaStreamDispatcherHostTest,
       GenerateTwoStreamsAndStopDeviceWhileWaitingForSecondStream) {
  blink::StreamControls controls(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  // Generate a second stream.
  EXPECT_CALL(*host_, OnStreamGenerationSuccess(kPageRequestId + 1, 0, 1));

  base::RunLoop run_loop1;
  host_->OnGenerateStream(kPageRequestId + 1, controls,
                          run_loop1.QuitClosure());

  // Stop the video stream device from stream 1 while waiting for the
  // second stream to be generated.
  host_->OnStopStreamDevice(host_->video_devices_[0].id,
                            host_->video_devices_[0].session_id());
  run_loop1.Run();

  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

TEST_F(MediaStreamDispatcherHostTest, CancelPendingStreams) {
  blink::StreamControls controls(false, true);

  base::RunLoop run_loop;

  // Create multiple GenerateStream requests.
  size_t streams = 5;
  for (size_t i = 1; i <= streams; ++i) {
    host_->OnGenerateStream(kPageRequestId + i, controls,
                            run_loop.QuitClosure());
  }

  media_stream_manager_->CancelAllRequests(kProcessId, kRenderId, kRequesterId);
  run_loop.RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, StopGeneratedStreams) {
  blink::StreamControls controls(false, true);

  SetupFakeUI(true);

  // Create first group of streams.
  size_t generated_streams = 3;
  for (size_t i = 0; i < generated_streams; ++i)
    GenerateStreamAndWaitForResult(kPageRequestId + i, controls);

  media_stream_manager_->CancelAllRequests(kProcessId, kRenderId, kRequesterId);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, CloseFromUI) {
  blink::StreamControls controls(false, true);

  base::Closure close_callback;
  media_stream_manager_->UseFakeUIFactoryForTests(base::Bind(
      [](base::Closure* close_callback) {
        std::unique_ptr<FakeMediaStreamUIProxy> stream_ui =
            std::make_unique<MockMediaStreamUIProxy>();
        EXPECT_CALL(*static_cast<MockMediaStreamUIProxy*>(stream_ui.get()),
                    MockOnStarted(_))
            .WillOnce(SaveArg<0>(close_callback));
        return stream_ui;
      },
      &close_callback));

  GenerateStreamAndWaitForResult(kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  ASSERT_FALSE(close_callback.is_null());
  EXPECT_CALL(*host_, OnDeviceStopSuccess());
  close_callback.Run();
  base::RunLoop().RunUntilIdle();
}

// Test that the observer is notified if a video device that is in use is
// being unplugged.
TEST_F(MediaStreamDispatcherHostTest, VideoDeviceUnplugged) {
  blink::StreamControls controls(true, true);
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls);
  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  stub_video_device_ids_.clear();

  base::RunLoop run_loop;
  EXPECT_CALL(*host_, OnDeviceStopSuccess())
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  media_stream_manager_->media_devices_manager()->OnDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  run_loop.Run();
}

// Test that changing the salt invalidates device IDs. Attempts to open an
// invalid device ID result in failure.
TEST_F(MediaStreamDispatcherHostTest, Salt) {
  SetupFakeUI(true);
  blink::StreamControls controls(false, true);

  // Generate first stream.
  GenerateStreamAndWaitForResult(kPageRequestId, controls);
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().id;
  EXPECT_TRUE(host_->video_devices_.front().group_id.has_value());
  const std::string group_id1 = *host_->video_devices_.front().group_id;
  EXPECT_FALSE(group_id1.empty());
  const base::UnguessableToken session_id1 =
      host_->video_devices_.front().session_id();

  // Generate second stream.
  OpenVideoDeviceAndWaitForResult(kPageRequestId, device_id1);
  const std::string device_id2 = host_->opened_device_.id;
  EXPECT_TRUE(host_->opened_device_.group_id.has_value());
  const std::string group_id2 = *host_->opened_device_.group_id;
  EXPECT_FALSE(group_id2.empty());
  const base::UnguessableToken session_id2 = host_->opened_device_.session_id();
  const std::string label2 = host_->label_;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_EQ(group_id1, group_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);

  // Reset salt and try to generate third stream with the invalidated device ID.
  browser_context_ = std::make_unique<TestBrowserContext>();
  EXPECT_CALL(*host_, OnDeviceOpenSuccess()).Times(0);
  OpenVideoDeviceAndWaitForFailure(kPageRequestId, device_id1);
  // Last open device ID and session are from the second stream.
  EXPECT_EQ(session_id2, host_->opened_device_.session_id());
  EXPECT_EQ(device_id2, host_->opened_device_.id);
  EXPECT_EQ(group_id2, host_->opened_device_.group_id);
}

}  // namespace content
