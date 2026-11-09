// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/audio_service.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_processing.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/audio_processing.mojom-test-utils.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom-test-utils.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/audio/public/mojom/audio_service.mojom-test-utils.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "services/audio/public/mojom/ml_model_manager.mojom.h"
#include "services/audio/public/mojom/system_info.mojom.h"
#include "services/audio/service_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::UnorderedElementsAre;

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

enum class SystemAec { kDisabled, kEnabled };

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

  bool GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    device_names->push_back(media::AudioDeviceName::CreateDefault());
    return true;
  }

  struct CreatedStream {
    media::AudioParameters params;
    std::string device_id;
  };

  media::AudioInputStream* MakeLowLatencyInputStream(
      const media::AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override {
    created_streams.push_back({params, device_id});
    return new NiceMock<MockAudioInputStream>(this);
  }

  void ReleaseInputStream(media::AudioInputStream* stream) override {
    media::FakeAudioManager::ReleaseInputStream(stream);
    if (input_stream_count() == 0 && on_all_streams_closed_) {
      std::move(on_all_streams_closed_).Run();
    }
  }

  std::vector<CreatedStream> created_streams;
  media::AudioParameters default_device_params = kInputParamsDefault;
  base::OnceClosure on_all_streams_closed_;

 private:
  media::FakeAudioLogFactory log_factory_;
};

class AudioServiceBrowserTestBase : public ContentBrowserTest {
 public:
  explicit AudioServiceBrowserTestBase(
      std::vector<base::test::FeatureRef> extra_enabled_features = {}) {
    extra_enabled_features.push_back(features::kUserMediaCaptureOnFocus);
    scoped_feature_list_.InitWithFeatures(
        extra_enabled_features,
        // Because we will override the AudioService with one that lives in the
        // current process, we disable out-of-process AudioService.
        {features::kAudioServiceOutOfProcess});
  }
  ~AudioServiceBrowserTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // Automatically grant device permission.
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void SetUp() override {
    // Since we are going to override AudioManager, do not use fake streams.
    SetUseFakeMediaStreamDevices(false);
    ContentBrowserTest::SetUp();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class AudioServiceBrowserTest
    : public AudioServiceBrowserTestBase,
      public ::testing::WithParamInterface<SystemAecAvailability> {
 public:
  AudioServiceBrowserTest() = default;
  ~AudioServiceBrowserTest() override = default;

  enum ExpectedBehaviour {
    kOverconstrained,
    kUnprocessedOrRendererProcessed,
    kAudioServiceProcessed,
    kLoopbackAec,
    kSystemAec
  };

 protected:
  void TestGetUserMedia(ExpectedBehaviour expected_behaviour,
                        std::string_view gum_config);

  bool SystemAecAvailable() { return GetParam() == kSystemAecAvailable; }

  bool AudioProcessorAecAvailable() {
    // AEC never runs in Chrome on Android.
    return !BUILDFLAG(IS_ANDROID);
  }

  bool LoopbackAecAvailable() {
    return media::IsSystemLoopbackAsAecReferenceEnabled();
  }
};

class AudioProcessorControlsInterceptor
    : public media::mojom::AudioProcessorControlsInterceptorForTesting {
 public:
  explicit AudioProcessorControlsInterceptor(
      mojo::PendingReceiver<media::mojom::AudioProcessorControls>&
          controls_receiver)
      : client_receiver_(
            this,
            std::exchange(controls_receiver,
                          service_remote_.BindNewPipeAndPassReceiver())) {}
  ~AudioProcessorControlsInterceptor() override = default;

  media::mojom::AudioProcessorControls* GetForwardingInterface() override {
    return service_remote_.get();
  }

  void SetVoiceIsolation(bool enabled) override {
    voice_isolation_states_.push_back(enabled);
    if (on_set_voice_isolation_) {
      on_set_voice_isolation_.Run(enabled);
    }
    GetForwardingInterface()->SetVoiceIsolation(enabled);
  }

  std::vector<bool> voice_isolation_states_;
  base::RepeatingCallback<void(bool)> on_set_voice_isolation_;

 private:
  mojo::Remote<media::mojom::AudioProcessorControls> service_remote_;
  mojo::Receiver<media::mojom::AudioProcessorControls> client_receiver_;
};

class AudioStreamFactoryInterceptor
    : public media::mojom::AudioStreamFactoryInterceptorForTesting {
 public:
  explicit AudioStreamFactoryInterceptor(
      audio::mojom::AudioService* audio_service) {
    audio_service->BindStreamFactory(
        real_stream_factory_.BindNewPipeAndPassReceiver());
  }
  ~AudioStreamFactoryInterceptor() override = default;

  void Bind(mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  media::mojom::AudioStreamFactory* GetForwardingInterface() override {
    return real_stream_factory_.get();
  }

  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      uint32_t shared_memory_count,
      bool enable_agc,
      media::mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback callback) override {
    last_stream_had_controls_receiver_ =
        processing_config && processing_config->controls_receiver.is_valid();
    if (processing_config) {
      last_created_processing_settings_ = processing_config->settings;
      if (processing_config->controls_receiver.is_valid()) {
        controls_interceptors_.push_back(
            std::make_unique<AudioProcessorControlsInterceptor>(
                processing_config->controls_receiver));
      }
    } else {
      last_created_processing_settings_.reset();
    }

    GetForwardingInterface()->CreateInputStream(
        std::move(stream), std::move(client), std::move(observer),
        std::move(log), device_id, params, group_id, shared_memory_count,
        enable_agc, std::move(processing_config), std::move(callback));
  }

  std::optional<media::AudioProcessingSettings>
      last_created_processing_settings_;
  // True when the last `CreateInputStream()` call passed a valid
  // `AudioProcessorControls` receiver in `processing_config`.
  bool last_stream_had_controls_receiver_ = false;
  std::vector<std::unique_ptr<AudioProcessorControlsInterceptor>>
      controls_interceptors_;

 private:
  mojo::Remote<media::mojom::AudioStreamFactory> real_stream_factory_;
  mojo::ReceiverSet<media::mojom::AudioStreamFactory> receivers_;
};

// Overrides the AudioService with a new one which has a mocked AudioManager
// within its scope and intercepts AudioStreamFactory IPCs.
class AudioServiceOverride
    : public audio::mojom::AudioServiceInterceptorForTesting {
 public:
  AudioServiceOverride()
      : audio_service_(CreateAudioService()),
        stream_factory_interceptor_(audio_service_remote_.get()),
        audio_service_auto_reset_(OverrideAudioServiceForTesting(this)) {}

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

  ~AudioServiceOverride() override {
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

  audio::mojom::AudioService* GetForwardingInterface() override {
    return audio_service_remote_.get();
  }

  void BindStreamFactory(mojo::PendingReceiver<media::mojom::AudioStreamFactory>
                             receiver) override {
    stream_factory_interceptor_.Bind(std::move(receiver));
  }

  LocalMockAudioManager audio_manager_;
  mojo::Remote<audio::mojom::AudioService> audio_service_remote_;
  std::unique_ptr<audio::Service> audio_service_;
  AudioStreamFactoryInterceptor stream_factory_interceptor_;
  base::AutoReset<audio::mojom::AudioService*> audio_service_auto_reset_;
};

MATCHER_P2(CreatedStreamIs, device_id, system_aec, "") {
  const bool has_system_aec =
      (arg.params.effects() & media::AudioParameters::ECHO_CANCELLER) != 0;
  return arg.device_id == device_id &&
         has_system_aec == (system_aec == SystemAec::kEnabled);
}

// Tests that calling getUserMedia() with `gum_config` yields behaviour
// specified by `expected_behaviour`.
void AudioServiceBrowserTest::TestGetUserMedia(
    ExpectedBehaviour expected_behaviour,
    std::string_view gum_config) {
  AudioServiceOverride audio_service_override;
  audio_service_override.audio_manager_.default_device_params =
      SystemAecAvailable() ? kInputParamsSystemAec : kInputParamsDefault;

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  bool success_expected =
      expected_behaviour != ExpectedBehaviour::kOverconstrained;
  base::test::TestFuture<void> streams_closed_future;
  if (success_expected) {
    audio_service_override.audio_manager_.on_all_streams_closed_ =
        streams_closed_future.GetSequenceBoundCallback();
  }
  EXPECT_EQ(success_expected ? "success" : "OverconstrainedError",
            EvalJs(shell(), base::StringPrintf(
                                "navigator.mediaDevices.getUserMedia(%s)"
                                "    .then(stream => {"
                                "      const [track] = stream.getAudioTracks();"
                                "      track.stop();"
                                "      return 'success';"
                                "    }, e => e.name)",
                                std::string(gum_config).c_str())));
  if (success_expected) {
    // Wait for stream teardown on the audio thread to finish before reading
    // `created_streams`. Because `InputStream::Record()` (which opens the
    // loopback reference stream) precedes stream closure on the same Mojo
    // pipe, waiting for all streams to close guarantees that all stream
    // creations on the audio thread have completed and establishes a
    // happens-before barrier for reading `created_streams` on the UI thread.
    ASSERT_TRUE(streams_closed_future.Wait());
  }

  const auto& streams = audio_service_override.audio_manager_.created_streams;
  const auto& stream_factory =
      audio_service_override.stream_factory_interceptor_;
  const auto& processing_settings =
      stream_factory.last_created_processing_settings_;
  switch (expected_behaviour) {
    case kOverconstrained:
      // Constraint resolution rejected in Blink before reaching the Audio
      // Service: no OS input stream is opened and no Audio Service processing
      // is configured.
      EXPECT_THAT(streams, IsEmpty());
      EXPECT_FALSE(processing_settings.has_value());
      EXPECT_FALSE(stream_factory.last_stream_had_controls_receiver_);
      break;
    case kUnprocessedOrRendererProcessed:
      // Either audio processing is disabled or WebRTC APM runs in the renderer
      // (e.g. `remote-only` AEC): the Audio Service opens a raw input stream on
      // the default device without system AEC and without Audio Service APM
      // (`processing_config` is null).
      EXPECT_THAT(streams, ElementsAre(CreatedStreamIs("default",
                                                       SystemAec::kDisabled)));
      EXPECT_FALSE(processing_settings.has_value());
      EXPECT_FALSE(stream_factory.last_stream_had_controls_receiver_);
      break;
    case kAudioServiceProcessed:
      // Default software AEC (`echoCancellation: true` without system AEC) when
      // Chrome-wide AEC is enabled: the OS input stream is opened without
      // hardware/system AEC, and software APM runs in the Audio Service
      // (passing `AudioProcessingSettings` with `echo_cancellation`,
      // `noise_suppression`, and `automatic_gain_control` enabled by default
      // and a valid `AudioProcessorControls` receiver).
      EXPECT_THAT(streams, ElementsAre(CreatedStreamIs("default",
                                                       SystemAec::kDisabled)));
      ASSERT_TRUE(processing_settings.has_value());
      EXPECT_TRUE(processing_settings->echo_cancellation);
      EXPECT_TRUE(processing_settings->noise_suppression);
      EXPECT_TRUE(processing_settings->automatic_gain_control);
      EXPECT_TRUE(stream_factory.last_stream_had_controls_receiver_);
      break;
    case kSystemAec:
      // Platform/hardware AEC handles echo cancellation at the OS level: the
      // AudioManager opens the default input stream with the `ECHO_CANCELLER`
      // effect flag enabled, and Audio Service software APM is not used.
      EXPECT_THAT(streams,
                  ElementsAre(CreatedStreamIs("default", SystemAec::kEnabled)));
      EXPECT_FALSE(processing_settings.has_value());
      EXPECT_FALSE(stream_factory.last_stream_had_controls_receiver_);
      break;
    case kLoopbackAec:
      // System loopback AEC (`echoCancellation: "all"` with loopback reference
      // enabled): the Audio Service opens both the default capture stream and
      // the system-wide `"loopbackAllDevices"` reference stream, and runs
      // software APM in the Audio Service with both `echo_cancellation` and
      // `use_loopback_aec_reference` enabled.
      // TODO(http://crbug.com/445323379): system_aec should always be
      // SystemAec::kDisabled.
      EXPECT_THAT(
          streams,
          UnorderedElementsAre(
              CreatedStreamIs("default", SystemAecAvailable()
                                             ? SystemAec::kEnabled
                                             : SystemAec::kDisabled),
              CreatedStreamIs("loopbackAllDevices", SystemAec::kDisabled)));
      ASSERT_TRUE(processing_settings.has_value());
      EXPECT_TRUE(processing_settings->use_loopback_aec_reference);
      EXPECT_TRUE(processing_settings->echo_cancellation);
      EXPECT_TRUE(stream_factory.last_stream_had_controls_receiver_);
      break;
  }
}

IN_PROC_BROWSER_TEST_P(AudioServiceBrowserTest, GetUserMediaEcFalse) {
  TestGetUserMedia(kUnprocessedOrRendererProcessed,
                   "{audio: {echoCancellation: {exact: false}}}");
}

IN_PROC_BROWSER_TEST_P(AudioServiceBrowserTest, GetUserMediaEcTrue) {
  ExpectedBehaviour expected_behaviour;
  if (SystemAecAvailable()) {
    expected_behaviour = kSystemAec;
  } else if (media::IsChromeWideEchoCancellationEnabled()) {
    expected_behaviour = kAudioServiceProcessed;
  } else {
    expected_behaviour = kUnprocessedOrRendererProcessed;
  }
  TestGetUserMedia(expected_behaviour,
                   "{audio: {echoCancellation: {exact: true}}}");
}

IN_PROC_BROWSER_TEST_P(AudioServiceBrowserTest, GetUserMediaEcRemoteOnly) {
  TestGetUserMedia(AudioProcessorAecAvailable()
                       ? kUnprocessedOrRendererProcessed
                       : kOverconstrained,
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

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
class AudioServiceVoiceIsolationTest : public AudioServiceBrowserTestBase {
 public:
  AudioServiceVoiceIsolationTest()
      : AudioServiceBrowserTestBase({media::kChromeWideEchoCancellation,
                                     media::kWebRtcVoiceIsolationDenoiser}) {}
};

IN_PROC_BROWSER_TEST_F(AudioServiceVoiceIsolationTest,
                       VerifyVoiceIsolationEnabled) {
  AudioServiceOverride audio_service_override;

  // 1. Provide the voice isolation model to MlModelManager in AudioService.
  base::FilePath test_model_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_model_path));
  test_model_path = test_model_path.AppendASCII("media")
                        .AppendASCII("webrtc")
                        .AppendASCII("voice_isolation")
                        .AppendASCII("test_model_stateful_1_2_160_2.tflite");

  base::File test_model_file;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    test_model_file.Initialize(test_model_path,
                               base::File::FLAG_OPEN | base::File::FLAG_READ);
  }
  ASSERT_TRUE(test_model_file.IsValid());

  mojo::Remote<audio::mojom::MlModelManager> ml_model_manager;
  audio_service_override.audio_service_remote_->BindMlModelManager(
      ml_model_manager.BindNewPipeAndPassReceiver());
  ml_model_manager->SetModel(audio::mojom::MlModelType::kVoiceIsolationDenoiser,
                             std::move(test_model_file));

  // Wait until SystemInfo reports voice isolation support after the model file
  // is loaded and registered.
  mojo::Remote<audio::mojom::SystemInfo> system_info;
  audio_service_override.audio_service_remote_->BindSystemInfo(
      system_info.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(base::test::RunUntil([&] {
    base::test::TestFuture<const std::optional<media::AudioParameters>&> future;
    system_info->GetInputStreamParameters(
        media::AudioDeviceDescription::kDefaultDeviceId, future.GetCallback());
    return future.Get().has_value() &&
           (future.Get()->effects() &
            media::AudioParameters::VOICE_ISOLATION_SUPPORTED);
  }));

  // 2. Navigate to a test page capable of requesting getUserMedia().
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // 3. Request audio via JavaScript with Voice Isolation enabled.
  // Hold references on window to prevent garbage collection and verify that
  // track.getSettings().voiceIsolation is true.
  EXPECT_EQ(true, EvalJs(shell(),
                         "navigator.mediaDevices.getUserMedia({"
                         "  audio: {voiceIsolation: {exact: true}}"
                         "}).then(stream => {"
                         "  window.stream = stream;"
                         "  window.track = stream.getAudioTracks()[0];"
                         "  return window.track.getSettings().voiceIsolation;"
                         "})"));

  // Verify that Blink requested voice isolation across the Mojo IPC border.
  auto& stream_factory = audio_service_override.stream_factory_interceptor_;
  ASSERT_TRUE(stream_factory.last_created_processing_settings_.has_value());
  EXPECT_TRUE(
      stream_factory.last_created_processing_settings_->voice_isolation);
  EXPECT_TRUE(stream_factory.last_stream_had_controls_receiver_);
  ASSERT_EQ(stream_factory.controls_interceptors_.size(), 1u);
  auto& controls = *stream_factory.controls_interceptors_.back();

  // 4. Dynamically disable voice isolation via applyConstraints() on the active
  // track. Verify that track.getSettings().voiceIsolation updates to false and
  // the Mojo IPC SetVoiceIsolation(false) message is received.
  base::test::TestFuture<bool> disabled_future;
  controls.on_set_voice_isolation_ = disabled_future.GetRepeatingCallback();

  EXPECT_EQ(
      false,
      EvalJs(shell(),
             "window.track.applyConstraints({voiceIsolation: {exact: false}})"
             "    .then(() => window.track.getSettings().voiceIsolation)"));
  EXPECT_EQ(disabled_future.Take(), false);
  controls.on_set_voice_isolation_.Reset();

  // 5. Dynamically re-enable voice isolation via applyConstraints() on the
  // active track. Verify that track.getSettings().voiceIsolation updates back
  // to true and the Mojo IPC SetVoiceIsolation(true) message is received.
  base::test::TestFuture<bool> reenabled_future;
  controls.on_set_voice_isolation_ = reenabled_future.GetRepeatingCallback();

  EXPECT_EQ(
      true,
      EvalJs(shell(),
             "window.track.applyConstraints({voiceIsolation: {exact: true}})"
             "    .then(() => window.track.getSettings().voiceIsolation)"));
  EXPECT_EQ(reenabled_future.Take(), true);

  // Verify the exact sequence of dynamic IPC control messages received.
  EXPECT_THAT(controls.voice_isolation_states_, ElementsAre(false, true));

  // 6. Clean up track and wait for stream teardown on the audio thread.
  base::test::TestFuture<void> streams_closed_future;
  audio_service_override.audio_manager_.on_all_streams_closed_ =
      streams_closed_future.GetSequenceBoundCallback();
  EXPECT_EQ(true, EvalJs(shell(), "window.track.stop(); true;"));
  ASSERT_TRUE(streams_closed_future.Wait());
  EXPECT_THAT(audio_service_override.audio_manager_.created_streams,
              ElementsAre(CreatedStreamIs("default", SystemAec::kDisabled)));
}

IN_PROC_BROWSER_TEST_F(AudioServiceVoiceIsolationTest,
                       VoiceIsolationUnsupportedWithoutModelFails) {
  AudioServiceOverride audio_service_override;
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/simple_page.html")));

  // Without providing a model to MlModelManager, requesting exact voice
  // isolation must reject with OverconstrainedError.
  constexpr char kCheckUnsupportedJs[] =
      "navigator.mediaDevices.getUserMedia({ "
      "  audio: { voiceIsolation: { exact: true } } "
      "}).then(() => 'unexpected_success', e => e.name);";

  EXPECT_EQ("OverconstrainedError", EvalJs(shell(), kCheckUnsupportedJs));
  EXPECT_THAT(audio_service_override.audio_manager_.created_streams, IsEmpty());
}
#endif  // BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

}  // namespace content
