// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/audio_service.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/audio/public/mojom/testing_api.mojom.h"
#include "services/audio/service_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

const media::AudioParameters kInputParamsDefault =
    media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                           media::ChannelLayoutConfig::Mono(),
                           48000,
                           /*change depending on audio processor?*/ 480);

media::AudioParameters AddSystemAec(media::AudioParameters params) {
  params.set_effects(media::AudioParameters::ECHO_CANCELLER);
  return params;
}

const media::AudioParameters kInputParamsSystemAec =
    AddSystemAec(kInputParamsDefault);

enum SystemAecAvailability { kSystemAecNotAvailable, kSystemAecAvailable };

}  // namespace

namespace content {

class MockAudioInputStream : public media::AudioInputStream {
 public:
  explicit MockAudioInputStream(media::AudioManagerBase* audio_manager_base)
      : audio_manager_base_(audio_manager_base) {}
  MOCK_METHOD0(Open, OpenOutcome());
  MOCK_METHOD1(Start, void(AudioInputCallback* callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(GetMaxVolume, double());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD0(GetVolume, double());
  MOCK_METHOD1(SetAutomaticGainControl, bool(bool enabled));
  MOCK_METHOD0(GetAutomaticGainControl, bool());
  MOCK_METHOD0(IsMuted, bool());
  MOCK_METHOD1(SetOutputDeviceForAec,
               void(const std::string& output_device_id));
  void Close() override { audio_manager_base_->ReleaseInputStream(this); }
  raw_ptr<media::AudioManagerBase> audio_manager_base_;
};

class MockAudioLog : public media::AudioLog {
 public:
  MockAudioLog() = default;
  MOCK_METHOD2(OnCreated,
               void(const media::AudioParameters& params,
                    const std::string& device_id));

  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnSetVolume, void(double));
  MOCK_METHOD1(OnProcessingStateChanged, void(const std::string&));
  MOCK_METHOD1(OnLogMessage, void(const std::string& message));
};

class LocalMockAudioManager : public media::FakeAudioManager {
 public:
  LocalMockAudioManager()
      : media::FakeAudioManager(std::make_unique<media::TestAudioThread>(true),
                                &log_factory_) {}
  ~LocalMockAudioManager() override = default;

  media::AudioParameters GetInputStreamParameters(
      const std::string& device_id) override {
    return device_id == "default" ? default_device_params : kInputParamsDefault;
  }

  bool HasAudioInputDevices() override { return true; }
  std::unique_ptr<media::AudioLog> CreateAudioLog(
      media::AudioLogFactory::AudioComponent component,
      int component_id) override {
    return std::make_unique<NiceMock<MockAudioLog>>();
  }

  void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    device_names->push_back(media::AudioDeviceName::CreateDefault());
  }

  media::AudioInputStream* MakeLowLatencyInputStream(
      const media::AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override {
    OnMakeLowLatencyInputStream(params, device_id, log_callback);
    return new NiceMock<MockAudioInputStream>(this);
  }

  MOCK_METHOD(void,
              OnMakeLowLatencyInputStream,
              (const media::AudioParameters& params,
               const std::string& device_id,
               const LogCallback& log_callback));

  media::AudioParameters default_device_params = kInputParamsDefault;

 private:
  media::FakeAudioLogFactory log_factory_;
};

class AudioServiceBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<SystemAecAvailability> {
 public:
  AudioServiceBrowserTest() {
    // Automatically grant device permission.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeUIForMediaStream);
    scoped_feature_list_.InitWithFeatures(
        {features::kUserMediaCaptureOnFocus},
        // Because we will override the AudioService with one that lives in the
        // current process, we disable out-of-process AudioService.
        {features::kAudioServiceOutOfProcess});
  }
  ~AudioServiceBrowserTest() override = default;

  void SetUp() override {
    // Since we are going to override AudioManager, do not use fake streams.
    SetUseFakeMediaStreamDevices(false);
    ContentBrowserTest::SetUp();
  }

  enum ExpectedBehaviour {
    kOverconstrained,
    kUnprocessed,
    kProcessed,
    kLoopbackAec,
    kSystemAec
  };

 protected:
  void TestGetUserMedia(ExpectedBehaviour expected_behaviour,
                        std::string gum_config);

  bool SystemAecAvailable() { return GetParam() == kSystemAecAvailable; }

  bool AudioProcessorAecAvailable() {
    // AEC never runs in Chrome on Android.
    return !BUILDFLAG(IS_ANDROID);
  }

  bool LoopbackAecAvailable() {
    return media::IsSystemLoopbackAsAecReferenceEnabled();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Overrides the AudioService with a new one which has a mocked AudioManager
// within its scope.
class AudioServiceOverride {
 public:
  AudioServiceOverride()
      : audio_service_(CreateAudioService()),
        audio_service_auto_reset_(
            OverrideAudioServiceForTesting(audio_service_remote_.get())) {}

  std::unique_ptr<audio::Service> CreateAudioService() {
    std::unique_ptr<audio::Service> audio_service;
    mojo::PendingReceiver<audio::mojom::AudioService> pending_receiver =
        audio_service_remote_.BindNewPipeAndPassReceiver();

    base::WaitableEvent created_audio_service_event;
    audio_manager_.GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WaitableEvent* created_audio_service_event,
               media::AudioManager* audio_manager,
               mojo::PendingReceiver<audio::mojom::AudioService>
                   pending_receiver,
               std::unique_ptr<audio::Service>* returned_audio_service) {
              (*returned_audio_service) = audio::CreateEmbeddedService(
                  audio_manager, std::move(pending_receiver));
              created_audio_service_event->Signal();
            },
            &created_audio_service_event, &audio_manager_,
            std::move(pending_receiver), &audio_service));
    EXPECT_TRUE(created_audio_service_event.TimedWait(base::Seconds(1)));
    return audio_service;
  }

  ~AudioServiceOverride() {
    base::WaitableEvent destroyed_audio_service_event;
    audio_manager_.GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WaitableEvent* destroyed_audio_service_event,
               std::unique_ptr<audio::Service> audio_service) {
              audio_service.reset();
              destroyed_audio_service_event->Signal();
            },
            &destroyed_audio_service_event, std::move(audio_service_)));
    EXPECT_TRUE(destroyed_audio_service_event.TimedWait(base::Seconds(1)));
    audio_manager_.Shutdown();
  }

  StrictMock<LocalMockAudioManager> audio_manager_;
  mojo::Remote<audio::mojom::AudioService> audio_service_remote_;
  std::unique_ptr<audio::Service> audio_service_;
  base::AutoReset<audio::mojom::AudioService*> audio_service_auto_reset_;
};

MATCHER_P(AudioParamsHasSystemAec, system_aec, "") {
  return !!(arg.effects() & media::AudioParameters::ECHO_CANCELLER) ==
         system_aec;
}

// Tests that calling getUserMedia() with `gum_config` yields behaviour
// specified by `expected_behaviour`.
void AudioServiceBrowserTest::TestGetUserMedia(
    ExpectedBehaviour expected_behaviour,
    std::string gum_config) {
  AudioServiceOverride audio_service_override;
  audio_service_override.audio_manager_.default_device_params =
      SystemAecAvailable() ? kInputParamsSystemAec : kInputParamsDefault;

  switch (expected_behaviour) {
    case kOverconstrained:
      // The stream will not be created.
      break;
    case kUnprocessed:
    case kProcessed:
      // TODO(fhernqvist): Differentiate between processed and unprocessed.
      // Alternatives include:
      // * Looking for AudioProcessor::Create in the MediaInternals logs
      // * Expecting a buffer size specified by APM
      EXPECT_CALL(audio_service_override.audio_manager_,
                  OnMakeLowLatencyInputStream(AudioParamsHasSystemAec(false),
                                              "default", _));
      break;
    case kSystemAec:
      EXPECT_CALL(audio_service_override.audio_manager_,
                  OnMakeLowLatencyInputStream(AudioParamsHasSystemAec(true),
                                              "default", _));
      break;
    case kLoopbackAec:
      // TODO(http://crbug.com/445323379): HasSystemAec should always be false
      EXPECT_CALL(
          audio_service_override.audio_manager_,
          OnMakeLowLatencyInputStream(
              AudioParamsHasSystemAec(SystemAecAvailable()), "default", _));
      EXPECT_CALL(audio_service_override.audio_manager_,
                  OnMakeLowLatencyInputStream(AudioParamsHasSystemAec(false),
                                              "loopbackAllDevices", _));

      break;
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  bool success_expected =
      expected_behaviour != ExpectedBehaviour::kOverconstrained;
  EXPECT_EQ(success_expected,
            ExecJs(shell(), base::StringPrintf(
                                "navigator.mediaDevices.getUserMedia(%s)"
                                "    .then(stream => {"
                                "      const [track] = stream.getAudioTracks();"
                                "      track.stop();"
                                "      return true;"
                                "    })",
                                gum_config)));
}

IN_PROC_BROWSER_TEST_P(AudioServiceBrowserTest, GetUserMediaEcFalse) {
  TestGetUserMedia(kUnprocessed, "{audio: {echoCancellation: {exact: false}}}");
}

IN_PROC_BROWSER_TEST_P(AudioServiceBrowserTest, GetUserMediaEcTrue) {
  TestGetUserMedia(SystemAecAvailable() ? kSystemAec : kProcessed,
                   "{audio: {echoCancellation: {exact: true}}}");
}

IN_PROC_BROWSER_TEST_P(AudioServiceBrowserTest, GetUserMediaEcRemoteOnly) {
  TestGetUserMedia(AudioProcessorAecAvailable() ? kProcessed : kOverconstrained,
                   "{audio: {echoCancellation: {exact: \"remote-only\"}}}");
}

IN_PROC_BROWSER_TEST_P(AudioServiceBrowserTest, GetUserMediaEcAll) {
  ExpectedBehaviour expected_behaviour = kOverconstrained;
  if (LoopbackAecAvailable()) {
    expected_behaviour = kLoopbackAec;
  } else if (SystemAecAvailable()) {
    expected_behaviour = kSystemAec;
  }
  TestGetUserMedia(expected_behaviour,
                   "{audio: {echoCancellation: {exact: \"all\"}}}");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AudioServiceBrowserTest,
    ::testing::Values(kSystemAecAvailable, kSystemAecNotAvailable),
    [](const testing::TestParamInfo<SystemAecAvailability>& info) {
      switch (info.param) {
        case kSystemAecNotAvailable:
          return "SystemAecNotAvailable";
        case kSystemAecAvailable:
          return "SystemAecAvailable";
      }
    });

}  // namespace content
