// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities.h"

#include <math.h>

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/learning/common/media_learning_tasks.h"
#include "media/learning/common/target_histogram.h"
#include "media/learning/mojo/public/mojom/learning_task_controller.mojom-blink.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-blink.h"
#include "media/mojo/mojom/media_types.mojom-blink.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom-blink.h"
#include "media/mojo/mojom/watch_time_recorder.mojom-blink.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom-blink.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_capabilities_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_decoding_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_encoding_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_configuration.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_factory.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-actions.h"
#include "ui/gfx/geometry/size.h"

using ::media::learning::FeatureValue;
using ::media::learning::ObservationCompletion;
using ::media::learning::TargetValue;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Unused;

namespace blink {

namespace {

// Simulating the browser-side service.
class MockPerfHistoryService
    : public media::mojom::blink::VideoDecodePerfHistory {
 public:
  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<media::mojom::blink::VideoDecodePerfHistory>(
            std::move(handle)));
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockPerfHistoryService::OnConnectionError, base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  // media::mojom::blink::VideoDecodePerfHistory implementation:
  MOCK_METHOD2(GetPerfInfo,
               void(media::mojom::blink::PredictionFeaturesPtr features,
                    GetPerfInfoCallback got_info_cb));

 private:
  mojo::Receiver<media::mojom::blink::VideoDecodePerfHistory> receiver_{this};
};

class MockWebrtcPerfHistoryService
    : public media::mojom::blink::WebrtcVideoPerfHistory {
 public:
  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<media::mojom::blink::WebrtcVideoPerfHistory>(
            std::move(handle)));
    receiver_.set_disconnect_handler(
        base::BindOnce(&MockWebrtcPerfHistoryService::OnConnectionError,
                       base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  // media::mojom::blink::WebrtcVideoPerfHistory implementation:
  MOCK_METHOD3(GetPerfInfo,
               void(media::mojom::blink::WebrtcPredictionFeaturesPtr features,
                    int frames_per_second,
                    GetPerfInfoCallback got_info_cb));

 private:
  mojo::Receiver<media::mojom::blink::WebrtcVideoPerfHistory> receiver_{this};
};

class MockLearningTaskControllerService
    : public media::learning::mojom::blink::LearningTaskController {
 public:
  void BindRequest(mojo::PendingReceiver<
                   media::learning::mojom::blink::LearningTaskController>
                       pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&MockLearningTaskControllerService::OnConnectionError,
                       base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  bool is_bound() const { return receiver_.is_bound(); }

  // media::mojom::blink::LearningTaskController implementation:
  MOCK_METHOD3(BeginObservation,
               void(const base::UnguessableToken& id,
                    const WTF::Vector<FeatureValue>& features,
                    const std::optional<TargetValue>& default_target));
  MOCK_METHOD2(CompleteObservation,
               void(const base::UnguessableToken& id,
                    const ObservationCompletion& completion));
  MOCK_METHOD1(CancelObservation, void(const base::UnguessableToken& id));
  MOCK_METHOD2(UpdateDefaultTarget,
               void(const base::UnguessableToken& id,
                    const std::optional<TargetValue>& default_target));
  MOCK_METHOD2(PredictDistribution,
               void(const WTF::Vector<FeatureValue>& features,
                    PredictDistributionCallback callback));

 private:
  mojo::Receiver<media::learning::mojom::blink::LearningTaskController>
      receiver_{this};
};

class FakeMediaMetricsProvider
    : public media::mojom::blink::MediaMetricsProvider {
 public:
  // Raw pointers to services owned by the test.
  FakeMediaMetricsProvider(
      MockLearningTaskControllerService* bad_window_service,
      MockLearningTaskControllerService* nnr_service)
      : bad_window_service_(bad_window_service), nnr_service_(nnr_service) {}

  ~FakeMediaMetricsProvider() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<media::mojom::blink::MediaMetricsProvider>(
            std::move(handle)));
    receiver_.set_disconnect_handler(base::BindOnce(
        &FakeMediaMetricsProvider::OnConnectionError, base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  // mojom::WatchTimeRecorderProvider implementation:
  void AcquireWatchTimeRecorder(
      media::mojom::blink::PlaybackPropertiesPtr properties,
      mojo::PendingReceiver<media::mojom::blink::WatchTimeRecorder> receiver)
      override {
    FAIL();
  }
  void AcquireVideoDecodeStatsRecorder(
      mojo::PendingReceiver<media::mojom::blink::VideoDecodeStatsRecorder>
          receiver) override {
    FAIL();
  }
  void AcquireLearningTaskController(
      const WTF::String& taskName,
      mojo::PendingReceiver<
          media::learning::mojom::blink::LearningTaskController>
          pending_receiver) override {
    if (taskName == media::learning::tasknames::kConsecutiveBadWindows) {
      bad_window_service_->BindRequest(std::move(pending_receiver));
      return;
    }

    if (taskName == media::learning::tasknames::kConsecutiveNNRs) {
      nnr_service_->BindRequest(std::move(pending_receiver));
      return;
    }
    FAIL();
  }
  void AcquirePlaybackEventsRecorder(
      mojo::PendingReceiver<media::mojom::blink::PlaybackEventsRecorder>
          receiver) override {
    FAIL();
  }
  void Initialize(bool is_mse,
                  media::mojom::MediaURLScheme url_scheme,
                  media::mojom::MediaStreamType media_stream_type) override {}
  void OnStarted(media::mojom::blink::PipelineStatusPtr status) override {}
  void OnError(media::mojom::blink::PipelineStatusPtr status) override {}
  void OnFallback(::media::mojom::blink::PipelineStatusPtr status) override {}
  void SetIsEME() override {}
  void SetTimeToMetadata(base::TimeDelta elapsed) override {}
  void SetTimeToFirstFrame(base::TimeDelta elapsed) override {}
  void SetTimeToPlayReady(base::TimeDelta elapsed) override {}
  void SetContainerName(
      media::mojom::blink::MediaContainerName container_name) override {}
  void SetRendererType(
      media::mojom::blink::RendererType renderer_type) override {}
  void SetKeySystem(const String& key_system) override {}
  void SetHasWaitingForKey() override {}
  void SetIsHardwareSecure() override {}
  void SetHasPlayed() override {}
  void SetHaveEnough() override {}
  void SetHasAudio(media::mojom::AudioCodec audio_codec) override {}
  void SetHasVideo(media::mojom::VideoCodec video_codec) override {}
  void SetVideoPipelineInfo(
      media::mojom::blink::VideoPipelineInfoPtr info) override {}
  void SetAudioPipelineInfo(
      media::mojom::blink::AudioPipelineInfoPtr info) override {}

 private:
  mojo::Receiver<media::mojom::blink::MediaMetricsProvider> receiver_{this};
  raw_ptr<MockLearningTaskControllerService, DanglingUntriaged>
      bad_window_service_;
  raw_ptr<MockLearningTaskControllerService, DanglingUntriaged> nnr_service_;
};

// Simple helper for saving back-end callbacks for pending decodingInfo() calls.
// Callers can then manually fire the callbacks, gaining fine-grain control of
// the timing and order of their arrival.
class CallbackSaver {
 public:
  void SavePerfHistoryCallback(
      media::mojom::blink::PredictionFeaturesPtr features,
      MockPerfHistoryService::GetPerfInfoCallback got_info_cb) {
    perf_history_cb_ = std::move(got_info_cb);
  }

  void SaveBadWindowCallback(
      Vector<media::learning::FeatureValue> features,
      MockLearningTaskControllerService::PredictDistributionCallback
          predict_cb) {
    bad_window_cb_ = std::move(predict_cb);
  }

  void SaveNnrCallback(
      Vector<media::learning::FeatureValue> features,
      MockLearningTaskControllerService::PredictDistributionCallback
          predict_cb) {
    nnr_cb_ = std::move(predict_cb);
  }

  void SaveGpuFactoriesNotifyCallback(base::OnceClosure cb) {
    gpu_factories_notify_cb_ = std::move(cb);
  }

  MockPerfHistoryService::GetPerfInfoCallback& perf_history_cb() {
    return perf_history_cb_;
  }

  MockLearningTaskControllerService::PredictDistributionCallback&
  bad_window_cb() {
    return bad_window_cb_;
  }

  MockLearningTaskControllerService::PredictDistributionCallback& nnr_cb() {
    return nnr_cb_;
  }

  base::OnceClosure& gpu_factories_notify_cb() {
    return gpu_factories_notify_cb_;
  }

 private:
  MockPerfHistoryService::GetPerfInfoCallback perf_history_cb_;
  MockLearningTaskControllerService::PredictDistributionCallback bad_window_cb_;
  MockLearningTaskControllerService::PredictDistributionCallback nnr_cb_;
  base::OnceClosure gpu_factories_notify_cb_;
};

class MockPlatform : public TestingPlatformSupport {
 public:
  MockPlatform() = default;
  ~MockPlatform() override = default;

  MOCK_METHOD0(GetGpuFactories, media::GpuVideoAcceleratorFactories*());
};

// This would typically be a test fixture, but we need it to be
// STACK_ALLOCATED() in order to use V8TestingScope, and we can't force that on
// whatever gtest class instantiates the fixture.
class MediaCapabilitiesTestContext {
  STACK_ALLOCATED();

 public:
  MediaCapabilitiesTestContext() {
    perf_history_service_ = std::make_unique<MockPerfHistoryService>();
    webrtc_perf_history_service_ =
        std::make_unique<MockWebrtcPerfHistoryService>();
    bad_window_service_ = std::make_unique<MockLearningTaskControllerService>();
    nnr_service_ = std::make_unique<MockLearningTaskControllerService>();
    fake_metrics_provider_ = std::make_unique<FakeMediaMetricsProvider>(
        bad_window_service_.get(), nnr_service_.get());

    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::MediaMetricsProvider::Name_,
                  base::BindRepeating(
                      &FakeMediaMetricsProvider::BindRequest,
                      base::Unretained(fake_metrics_provider_.get()))));

    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::VideoDecodePerfHistory::Name_,
                  base::BindRepeating(
                      &MockPerfHistoryService::BindRequest,
                      base::Unretained(perf_history_service_.get()))));

    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::WebrtcVideoPerfHistory::Name_,
                  base::BindRepeating(
                      &MockWebrtcPerfHistoryService::BindRequest,
                      base::Unretained(webrtc_perf_history_service_.get()))));

    media_capabilities_ = MediaCapabilities::mediaCapabilities(
        *v8_scope_.GetWindow().navigator());
  }

  ~MediaCapabilitiesTestContext() {
    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::MediaMetricsProvider::Name_, {}));

    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::VideoDecodePerfHistory::Name_, {}));

    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::WebrtcVideoPerfHistory::Name_, {}));
  }

  ExceptionState& GetExceptionState() { return v8_scope_.GetExceptionState(); }

  ScriptState* GetScriptState() const { return v8_scope_.GetScriptState(); }

  v8::Isolate* GetIsolate() const { return GetScriptState()->GetIsolate(); }

  MediaCapabilities* GetMediaCapabilities() const {
    return media_capabilities_.Get();
  }

  MockPerfHistoryService* GetPerfHistoryService() const {
    return perf_history_service_.get();
  }

  MockWebrtcPerfHistoryService* GetWebrtcPerfHistoryService() const {
    return webrtc_perf_history_service_.get();
  }

  MockLearningTaskControllerService* GetBadWindowService() const {
    return bad_window_service_.get();
  }

  MockLearningTaskControllerService* GetNnrService() const {
    return nnr_service_.get();
  }

  MockPlatform& GetMockPlatform() { return *mock_platform_; }

  void VerifyAndClearMockExpectations() {
    testing::Mock::VerifyAndClearExpectations(GetPerfHistoryService());
    testing::Mock::VerifyAndClearExpectations(GetWebrtcPerfHistoryService());
    testing::Mock::VerifyAndClearExpectations(GetNnrService());
    testing::Mock::VerifyAndClearExpectations(GetBadWindowService());
    testing::Mock::VerifyAndClearExpectations(&GetMockPlatform());
  }

 private:
  V8TestingScope v8_scope_;
  ScopedTestingPlatformSupport<MockPlatform> mock_platform_;
  std::unique_ptr<MockPerfHistoryService> perf_history_service_;
  std::unique_ptr<MockWebrtcPerfHistoryService> webrtc_perf_history_service_;
  std::unique_ptr<FakeMediaMetricsProvider> fake_metrics_provider_;
  Persistent<MediaCapabilities> media_capabilities_;
  std::unique_ptr<MockLearningTaskControllerService> bad_window_service_;
  std::unique_ptr<MockLearningTaskControllerService> nnr_service_;
};

// |kVideoContentType|, |kCodec|, and |kCodecProfile| must match.
const char kVideoContentType[] = "video/webm; codecs=\"vp09.00.10.08\"";
const char kAudioContentType[] = "audio/webm; codecs=\"opus\"";
const media::VideoCodecProfile kCodecProfile = media::VP9PROFILE_PROFILE0;
const media::VideoCodec kCodec = media::VideoCodec::kVP9;
const double kFramerate = 20.5;
const int kWidth = 3840;
const int kHeight = 2160;
const int kBitrate = 2391000;
const char kWebrtcVideoContentType[] = "video/VP9; profile-id=\"0\"";
const char kWebrtcAudioContentType[] = "audio/opus";

// Construct AudioConfig using the constants above.
template <class T>
T* CreateAudioConfig(const char content_type[], const char type[]) {
  auto* audio_config = MakeGarbageCollected<AudioConfiguration>();
  audio_config->setContentType(content_type);
  auto* decoding_config = MakeGarbageCollected<T>();
  decoding_config->setType(type);
  decoding_config->setAudio(audio_config);
  return decoding_config;
}

// Construct media-source AudioConfig using the constants above.
MediaDecodingConfiguration* CreateAudioDecodingConfig() {
  return CreateAudioConfig<MediaDecodingConfiguration>(kAudioContentType,
                                                       "media-source");
}

// Construct webrtc decoding AudioConfig using the constants above.
MediaDecodingConfiguration* CreateWebrtcAudioDecodingConfig() {
  return CreateAudioConfig<MediaDecodingConfiguration>(kWebrtcAudioContentType,
                                                       "webrtc");
}

// Construct webrtc decoding AudioConfig using the constants above.
MediaEncodingConfiguration* CreateWebrtcAudioEncodingConfig() {
  return CreateAudioConfig<MediaEncodingConfiguration>(kWebrtcAudioContentType,
                                                       "webrtc");
}

// Construct VideoConfig using the constants above.
template <class T>
T* CreateVideoConfig(const char content_type[], const char type[]) {
  auto* video_config = MakeGarbageCollected<VideoConfiguration>();
  video_config->setFramerate(kFramerate);
  video_config->setContentType(content_type);
  video_config->setWidth(kWidth);
  video_config->setHeight(kHeight);
  video_config->setBitrate(kBitrate);
  auto* decoding_config = MakeGarbageCollected<T>();
  decoding_config->setType(type);
  decoding_config->setVideo(video_config);
  return decoding_config;
}

// Construct media-source VideoConfig using the constants above.
MediaDecodingConfiguration* CreateDecodingConfig() {
  return CreateVideoConfig<MediaDecodingConfiguration>(kVideoContentType,
                                                       "media-source");
}

// Construct webrtc decoding VideoConfig using the constants above.
MediaDecodingConfiguration* CreateWebrtcDecodingConfig() {
  return CreateVideoConfig<MediaDecodingConfiguration>(kWebrtcVideoContentType,
                                                       "webrtc");
}

// Construct webrtc encoding VideoConfig using the constants above.
MediaEncodingConfiguration* CreateWebrtcEncodingConfig() {
  return CreateVideoConfig<MediaEncodingConfiguration>(kWebrtcVideoContentType,
                                                       "webrtc");
}

// Construct PredicitonFeatures matching the CreateDecodingConfig, using the
// constants above.
media::mojom::blink::PredictionFeatures CreateFeatures() {
  media::mojom::blink::PredictionFeatures features;
  features.profile =
      static_cast<media::mojom::blink::VideoCodecProfile>(kCodecProfile);
  features.video_size = gfx::Size(kWidth, kHeight);
  features.frames_per_sec = kFramerate;

  // Not set by any tests so far. Choosing sane defaults to mirror production
  // code.
  features.key_system = "";
  features.use_hw_secure_codecs = false;

  return features;
}

Vector<media::learning::FeatureValue> CreateFeaturesML() {
  media::mojom::blink::PredictionFeatures features = CreateFeatures();

  // FRAGILE: Order here MUST match order in
  // WebMediaPlayerImpl::UpdateSmoothnessHelper().
  // TODO(chcunningham): refactor into something more robust.
  Vector<media::learning::FeatureValue> ml_features(
      {media::learning::FeatureValue(static_cast<int>(kCodec)),
       media::learning::FeatureValue(kCodecProfile),
       media::learning::FeatureValue(kWidth),
       media::learning::FeatureValue(kFramerate)});

  return ml_features;
}

// Construct WebrtcPredicitonFeatures matching the CreateWebrtc{Decoding,
// Encoding}Config, using the constants above.
media::mojom::blink::WebrtcPredictionFeatures CreateWebrtcFeatures(
    bool is_decode) {
  media::mojom::blink::WebrtcPredictionFeatures features;
  features.is_decode_stats = is_decode;
  features.profile =
      static_cast<media::mojom::blink::VideoCodecProfile>(kCodecProfile);
  features.video_pixels = kWidth * kHeight;
  return features;
}

// Types of smoothness predictions.
enum class PredictionType {
  kDB,
  kBadWindow,
  kNnr,
  kGpuFactories,
};

// Makes a TargetHistogram with single count at |target_value|.
media::learning::TargetHistogram MakeHistogram(double target_value) {
  media::learning::TargetHistogram histogram;
  histogram += media::learning::TargetValue(target_value);
  return histogram;
}

// Makes DB (PerfHistoryService) callback for use with gtest WillOnce().
// Callback will verify |features| matches |expected_features| and run with
// provided values for |is_smooth| and |is_power_efficient|.
testing::Action<void(media::mojom::blink::PredictionFeaturesPtr,
                     MockPerfHistoryService::GetPerfInfoCallback)>
DbCallback(const media::mojom::blink::PredictionFeatures& expected_features,
           bool is_smooth,
           bool is_power_efficient) {
  return [=](media::mojom::blink::PredictionFeaturesPtr features,
             MockPerfHistoryService::GetPerfInfoCallback got_info_cb) {
    EXPECT_TRUE(features->Equals(expected_features));
    std::move(got_info_cb).Run(is_smooth, is_power_efficient);
  };
}

// Makes ML (LearningTaskControllerService) callback for use with gtest
// WillOnce(). Callback will verify |features| matches |expected_features| and
// run a TargetHistogram containing a single count for |histogram_target|.
testing::Action<void(
    const Vector<media::learning::FeatureValue>&,
    MockLearningTaskControllerService::PredictDistributionCallback predict_cb)>
MlCallback(const Vector<media::learning::FeatureValue>& expected_features,
           double histogram_target) {
  return [=](const Vector<media::learning::FeatureValue>& features,
             MockLearningTaskControllerService::PredictDistributionCallback
                 predict_cb) {
    EXPECT_EQ(features, expected_features);
    std::move(predict_cb).Run(MakeHistogram(histogram_target));
  };
}

// Makes DB (WebrtcPerfHistoryService) callback for use with gtest WillOnce().
// Callback will verify |features| and |framerate| matches |expected_features|
// and |expected_framreate| and run with provided values for |is_smooth|.
testing::Action<void(media::mojom::blink::WebrtcPredictionFeaturesPtr,
                     int,
                     MockWebrtcPerfHistoryService::GetPerfInfoCallback)>
WebrtcDbCallback(
    const media::mojom::blink::WebrtcPredictionFeatures& expected_features,
    int expected_framerate,
    bool is_smooth) {
  return [=](media::mojom::blink::WebrtcPredictionFeaturesPtr features,
             int framerate,
             MockWebrtcPerfHistoryService::GetPerfInfoCallback got_info_cb) {
    EXPECT_TRUE(features->Equals(expected_features));
    EXPECT_EQ(framerate, expected_framerate);
    std::move(got_info_cb).Run(is_smooth);
  };
}

testing::Action<void(base::OnceClosure)> GpuFactoriesNotifyCallback() {
  return [](base::OnceClosure cb) { std::move(cb).Run(); };
}

// Helper to constructs field trial params with given ML prediction thresholds.
base::FieldTrialParams MakeMlParams(double bad_window_threshold,
                                    double nnr_threshold) {
  base::FieldTrialParams params;
  params[MediaCapabilities::kLearningBadWindowThresholdParamName] =
      base::NumberToString(bad_window_threshold);
  params[MediaCapabilities::kLearningNnrThresholdParamName] =
      base::NumberToString(nnr_threshold);
  return params;
}

// Wrapping decodingInfo() call for readability. Await resolution of the promise
// and return its info.
MediaCapabilitiesInfo* DecodingInfo(
    const MediaDecodingConfiguration* decoding_config,
    MediaCapabilitiesTestContext* context) {
  ScriptPromiseUntyped promise = context->GetMediaCapabilities()->decodingInfo(
      context->GetScriptState(), decoding_config, context->GetExceptionState());

  ScriptPromiseTester tester(context->GetScriptState(), promise);
  tester.WaitUntilSettled();

  CHECK(!tester.IsRejected()) << " Cant get info from rejected promise.";

  return NativeValueTraits<MediaCapabilitiesInfo>::NativeValue(
      context->GetIsolate(), tester.Value().V8Value(),
      context->GetExceptionState());
}

// Wrapping encodingInfo() call for readability. Await resolution of the promise
// and return its info.
MediaCapabilitiesInfo* EncodingInfo(
    const MediaEncodingConfiguration* encoding_config,
    MediaCapabilitiesTestContext* context) {
  ScriptPromiseUntyped promise = context->GetMediaCapabilities()->encodingInfo(
      context->GetScriptState(), encoding_config, context->GetExceptionState());

  ScriptPromiseTester tester(context->GetScriptState(), promise);
  tester.WaitUntilSettled();

  CHECK(!tester.IsRejected()) << " Cant get info from rejected promise.";

  return NativeValueTraits<MediaCapabilitiesInfo>::NativeValue(
      context->GetIsolate(), tester.Value().V8Value(),
      context->GetExceptionState());
}
}  // namespace

TEST(MediaCapabilitiesTests, BasicAudio) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  const MediaDecodingConfiguration* kDecodingConfig =
      CreateAudioDecodingConfig();
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
}

// Other tests will assume these match. Test to be sure they stay in sync.
TEST(MediaCapabilitiesTests, ConfigMatchesFeatures) {
  test::TaskEnvironment task_environment;
  const MediaDecodingConfiguration* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();

  EXPECT_TRUE(kDecodingConfig->video()->contentType().Contains("vp09.00"));
  EXPECT_EQ(static_cast<media::VideoCodecProfile>(kFeatures.profile),
            media::VP9PROFILE_PROFILE0);
  EXPECT_EQ(kCodecProfile, media::VP9PROFILE_PROFILE0);

  EXPECT_EQ(kDecodingConfig->video()->framerate(), kFeatures.frames_per_sec);
  EXPECT_EQ(kDecodingConfig->video()->width(),
            static_cast<uint32_t>(kFeatures.video_size.width()));
  EXPECT_EQ(kDecodingConfig->video()->height(),
            static_cast<uint32_t>(kFeatures.video_size.height()));
}

// Test that non-integer framerate isn't truncated by IPC.
// https://crbug.com/1024399
TEST(MediaCapabilitiesTests, NonIntegerFramerate) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      // Enabled features.
      {},
      // Disabled ML predictions + GpuFactories (just use DB).
      {media::kMediaCapabilitiesQueryGpuFactories,
       media::kMediaLearningSmoothnessExperiment});

  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();

  // FPS for this test must not be a whole number. Assert to ensure the default
  // config meets that condition.
  ASSERT_NE(fmod(kDecodingConfig->video()->framerate(), 1), 0);

  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce([&](media::mojom::blink::PredictionFeaturesPtr features,
                    MockPerfHistoryService::GetPerfInfoCallback got_info_cb) {
        // Explicitly check for frames_per_sec equality.
        // PredictionFeatures::Equals() will not catch loss of precision if
        // frames_per_sec is made to be int (currently a double).
        EXPECT_EQ(features->frames_per_sec, kFramerate);

        // Check that other things match as well.
        EXPECT_TRUE(features->Equals(kFeatures));

        std::move(got_info_cb).Run(/*smooth*/ true, /*power_efficient*/ true);
      });

  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
}

// Test smoothness predictions from DB (PerfHistoryService).
TEST(MediaCapabilitiesTests, PredictWithJustDB) {
  test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      // Enabled features.
      {},
      // Disabled ML predictions + GpuFactories (just use DB).
      {media::kMediaCapabilitiesQueryGpuFactories,
       media::kMediaLearningSmoothnessExperiment});

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();

  // ML services should not be called for prediction.
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .Times(0);
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _)).Times(0);

  // DB alone (PerfHistoryService) should be called. Signal smooth=true and
  // power_efficient = false.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*power_eff*/ false));
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());

  // Verify DB call was made. ML services should not even be bound.
  testing::Mock::VerifyAndClearExpectations(context.GetPerfHistoryService());
  EXPECT_FALSE(context.GetBadWindowService()->is_bound());
  EXPECT_FALSE(context.GetNnrService()->is_bound());

  // Repeat test with inverted smooth and power_efficient results.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*power_eff*/ true));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, PredictPowerEfficientWithGpuFactories) {
  test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      // Enable GpuFactories for power predictions.
      {media::kMediaCapabilitiesQueryGpuFactories},
      // Disable ML predictions (may/may not be disabled by default).
      {media::kMediaLearningSmoothnessExperiment});

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();

  // Setup DB to return powerEfficient = false. We later verify that opposite
  // response from GpuFactories overrides the DB.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*power_eff*/ false));

  auto mock_gpu_factories =
      std::make_unique<media::MockGpuVideoAcceleratorFactories>(nullptr);
  ON_CALL(context.GetMockPlatform(), GetGpuFactories())
      .WillByDefault(Return(mock_gpu_factories.get()));

  // First, lets simulate the scenario where we ask before support is known. The
  // async path should notify us when the info arrives. We then get GpuFactroies
  // again and learn the config is supported.
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories()).Times(2);
  {
    // InSequence because we EXPECT two calls to IsDecoderSupportKnown with
    // different return values.
    InSequence s;
    EXPECT_CALL(*mock_gpu_factories, IsDecoderSupportKnown())
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_gpu_factories, NotifyDecoderSupportKnown(_))
        .WillOnce(GpuFactoriesNotifyCallback());

    // MediaCapabilities calls IsDecoderSupportKnown() once, and
    // GpuVideoAcceleratorFactories::IsDecoderConfigSupported() also calls it
    // once internally.
    EXPECT_CALL(*mock_gpu_factories, IsDecoderSupportKnown())
        .Times(2)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_gpu_factories, IsDecoderConfigSupported(_))
        .WillOnce(
            Return(media::GpuVideoAcceleratorFactories::Supported::kTrue));
  }

  // Info should be powerEfficient, preferring response of GpuFactories over
  // the DB.
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->powerEfficient());
  EXPECT_FALSE(info->smooth());
  context.VerifyAndClearMockExpectations();
  testing::Mock::VerifyAndClearExpectations(mock_gpu_factories.get());

  // Now expect a second query with support is already known to be false. Set
  // DB to respond with the opposite answer.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*power_eff*/ true));
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories());
  EXPECT_CALL(*mock_gpu_factories, IsDecoderSupportKnown())
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_gpu_factories, IsDecoderConfigSupported(_))
      .WillRepeatedly(
          Return(media::GpuVideoAcceleratorFactories::Supported::kFalse));

  // Info should be NOT powerEfficient, preferring response of GpuFactories over
  // the DB.
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->powerEfficient());
  EXPECT_FALSE(info->smooth());
  context.VerifyAndClearMockExpectations();
  testing::Mock::VerifyAndClearExpectations(mock_gpu_factories.get());
}

// Test with smoothness predictions coming solely from "bad window" ML service.
TEST(MediaCapabilitiesTests, PredictWithBadWindowMLService) {
  test::TaskEnvironment task_environment;
  // Enable ML predictions with thresholds. -1 disables the NNR predictor.
  const double kBadWindowThreshold = 2;
  const double kNnrThreshold = -1;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      // Enabled features w/ parameters
      {{media::kMediaLearningSmoothnessExperiment,
        MakeMlParams(kBadWindowThreshold, kNnrThreshold)}},
      // Disabled GpuFactories (use DB for power).
      {media::kMediaCapabilitiesQueryGpuFactories});

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();
  const Vector<media::learning::FeatureValue> kFeaturesML = CreateFeaturesML();

  // ML is enabled, but DB should still be called for power efficiency (false).
  // Its smoothness value (true) should be ignored in favor of ML prediction.
  // Only bad window service should be asked for a prediction. Expect
  // smooth=false because bad window prediction is equal to its threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _)).Times(0);
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  // NNR service should not be bound when NNR predictions disabled.
  EXPECT_FALSE(context.GetNnrService()->is_bound());
  context.VerifyAndClearMockExpectations();

  // Same as above, but invert all signals. Expect smooth=true because bad
  // window prediction is now less than its threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold - 0.25));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _)).Times(0);
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  EXPECT_FALSE(context.GetNnrService()->is_bound());
  context.VerifyAndClearMockExpectations();

  // Same as above, but predict zero bad windows. Expect smooth=true because
  // zero is below the threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, /* bad windows */ 0));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _)).Times(0);
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  EXPECT_FALSE(context.GetNnrService()->is_bound());
  context.VerifyAndClearMockExpectations();
}

// Test with smoothness predictions coming solely from "NNR" ML service.
TEST(MediaCapabilitiesTests, PredictWithNnrMLService) {
  test::TaskEnvironment task_environment;
  // Enable ML predictions with thresholds. -1 disables the bad window
  // predictor.
  const double kBadWindowThreshold = -1;
  const double kNnrThreshold = 5;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      // Enabled both ML services.
      {{media::kMediaLearningSmoothnessExperiment,
        MakeMlParams(kBadWindowThreshold, kNnrThreshold)}},
      // Disabled features (use DB for power efficiency)
      {media::kMediaCapabilitiesQueryGpuFactories});

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();
  const Vector<media::learning::FeatureValue> kFeaturesML = CreateFeaturesML();

  // ML is enabled, but DB should still be called for power efficiency (false).
  // Its smoothness value (true) should be ignored in favor of ML prediction.
  // Only NNR service should be asked for a prediction. Expect smooth=false
  // because NNR prediction is equal to its threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .Times(0);
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold));
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  // Bad window service should not be bound when NNR predictions disabled.
  EXPECT_FALSE(context.GetBadWindowService()->is_bound());
  context.VerifyAndClearMockExpectations();

  // Same as above, but invert all signals. Expect smooth=true because NNR
  // prediction is now less than its threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .Times(0);
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold - 0.01));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  EXPECT_FALSE(context.GetBadWindowService()->is_bound());
  context.VerifyAndClearMockExpectations();

  // Same as above, but predict zero NNRs. Expect smooth=true because zero is
  // below the threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .Times(0);
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, /* NNRs */ 0));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  EXPECT_FALSE(context.GetBadWindowService()->is_bound());
  context.VerifyAndClearMockExpectations();
}

// Test with combined smoothness predictions from both ML services.
TEST(MediaCapabilitiesTests, PredictWithBothMLServices) {
  test::TaskEnvironment task_environment;
  // Enable ML predictions with thresholds.
  const double kBadWindowThreshold = 2;
  const double kNnrThreshold = 1;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      // Enabled both ML services.
      {{media::kMediaLearningSmoothnessExperiment,
        MakeMlParams(kBadWindowThreshold, kNnrThreshold)}},
      // Disabled features (use DB for power efficiency)
      {media::kMediaCapabilitiesQueryGpuFactories});

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();
  const Vector<media::learning::FeatureValue> kFeaturesML = CreateFeaturesML();

  // ML is enabled, but DB should still be called for power efficiency (false).
  // Its smoothness value (true) should be ignored in favor of ML predictions.
  // Both ML services should be called for prediction. In both cases we exceed
  // the threshold, such that smooth=false.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold + 0.5));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold + 0.5));
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();

  // Make another call to DecodingInfo with one "bad window" prediction
  // indicating smooth=false, while nnr prediction indicates smooth=true. Verify
  // resulting info predicts false, as the logic should OR the false signals.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold + 0.5));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold / 2));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();

  // Same as above, but invert predictions from ML services. Outcome should
  // still be smooth=false (logic is ORed).
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold / 2));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold + 0.5));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();

  // This time both ML services agree smooth=true while DB predicts
  // smooth=false. Expect info->smooth() = true, as only ML predictions matter
  // when ML experiment enabled.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold / 2));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold / 2));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();

  // Same as above, but with ML services predicting exactly their respective
  // thresholds. Now expect info->smooth() = false - reaching the threshold is
  // considered not smooth.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();
}

// Simulate a call to DecodingInfo with smoothness predictions arriving in the
// specified |callback_order|. Ensure that promise resolves correctly only after
// all callbacks have arrived.
void RunCallbackPermutationTest(std::vector<PredictionType> callback_order) {
  // Enable ML predictions with thresholds.
  const double kBadWindowThreshold = 2;
  const double kNnrThreshold = 3;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      // Enabled features w/ parameters
      {{media::kMediaLearningSmoothnessExperiment,
        MakeMlParams(kBadWindowThreshold, kNnrThreshold)},
       {media::kMediaCapabilitiesQueryGpuFactories, {}}},
      // Disabled features.
      {});

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  auto mock_gpu_factories =
      std::make_unique<media::MockGpuVideoAcceleratorFactories>(nullptr);

  // DB and both ML services should be called. Save their callbacks.
  CallbackSaver cb_saver;
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(Invoke(&cb_saver, &CallbackSaver::SavePerfHistoryCallback));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(Invoke(&cb_saver, &CallbackSaver::SaveBadWindowCallback));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(Invoke(&cb_saver, &CallbackSaver::SaveNnrCallback));

  // GpuFactories should also be called. Set it up to be async with arrival of
  // support info. Save the "notify" callback.
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .WillRepeatedly(Return(mock_gpu_factories.get()));
  {
    // InSequence because we EXPECT two calls to IsDecoderSupportKnown with
    // different return values.
    InSequence s;
    EXPECT_CALL(*mock_gpu_factories, IsDecoderSupportKnown())
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_gpu_factories, NotifyDecoderSupportKnown(_))
        .WillOnce(
            Invoke(&cb_saver, &CallbackSaver::SaveGpuFactoriesNotifyCallback));
    // MediaCapabilities calls IsDecoderSupportKnown() once, and
    // GpuVideoAcceleratorFactories::IsDecoderConfigSupported() also calls it
    // once internally.
    EXPECT_CALL(*mock_gpu_factories, IsDecoderSupportKnown())
        .Times(2)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_gpu_factories, IsDecoderConfigSupported(_))
        .WillRepeatedly(
            Return(media::GpuVideoAcceleratorFactories::Supported::kFalse));
  }

  // Call decodingInfo() to kick off the calls to prediction services.
  ScriptPromiseUntyped promise = context.GetMediaCapabilities()->decodingInfo(
      context.GetScriptState(), kDecodingConfig, context.GetExceptionState());
  ScriptPromiseTester tester(context.GetScriptState(), promise);

  // Callbacks should all be saved after mojo's pending tasks have run.
  test::RunPendingTasks();
  ASSERT_TRUE(cb_saver.perf_history_cb() && cb_saver.bad_window_cb() &&
              cb_saver.nnr_cb() && cb_saver.gpu_factories_notify_cb());

  // Complete callbacks in whatever order.
  for (size_t i = 0; i < callback_order.size(); ++i) {
    switch (callback_order[i]) {
      case PredictionType::kDB:
        std::move(cb_saver.perf_history_cb()).Run(true, true);
        break;
      case PredictionType::kBadWindow:
        std::move(cb_saver.bad_window_cb())
            .Run(MakeHistogram(kBadWindowThreshold - 0.25));
        break;
      case PredictionType::kNnr:
        std::move(cb_saver.nnr_cb()).Run(MakeHistogram(kNnrThreshold + 0.5));
        break;
      case PredictionType::kGpuFactories:
        std::move(cb_saver.gpu_factories_notify_cb()).Run();
        break;
    }

    // Give callbacks/tasks a chance to run.
    test::RunPendingTasks();

    // Promise should only be resolved once the final callback has run.
    if (i < callback_order.size() - 1) {
      ASSERT_FALSE(tester.IsFulfilled());
    } else {
      ASSERT_TRUE(tester.IsFulfilled());
    }
  }

  ASSERT_FALSE(tester.IsRejected()) << " Cant get info from rejected promise.";
  MediaCapabilitiesInfo* info =
      NativeValueTraits<MediaCapabilitiesInfo>::NativeValue(
          context.GetIsolate(), tester.Value().V8Value(),
          context.GetExceptionState());

  // Smooth=false because NNR prediction exceeds threshold.
  EXPECT_FALSE(info->smooth());
  // DB predicted power_efficient = true, but GpuFactories overrides w/ false.
  EXPECT_FALSE(info->powerEfficient());
}

// Test that decodingInfo() behaves correctly for all orderings/timings of the
// underlying prediction services.
TEST(MediaCapabilitiesTests, PredictionCallbackPermutations) {
  test::TaskEnvironment task_environment;
  std::vector<PredictionType> callback_order(
      {PredictionType::kDB, PredictionType::kBadWindow, PredictionType::kNnr,
       PredictionType::kGpuFactories});
  do {
    RunCallbackPermutationTest(callback_order);
  } while (std::next_permutation(callback_order.begin(), callback_order.end()));
}

// WebRTC decodingInfo tests.
TEST(MediaCapabilitiesTests, WebrtcDecodingBasicAudio) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));

  const MediaDecodingConfiguration* kDecodingConfig =
      CreateWebrtcAudioDecodingConfig();
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, WebrtcDecodingUnsupportedAudio) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));

  const MediaDecodingConfiguration* kDecodingConfig =
      CreateAudioConfig<MediaDecodingConfiguration>("audio/FooCodec", "webrtc");
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
}

// Other tests will assume these match. Test to be sure they stay in sync.
TEST(MediaCapabilitiesTests, WebrtcConfigMatchesFeatures) {
  test::TaskEnvironment task_environment;
  const MediaDecodingConfiguration* kDecodingConfig =
      CreateWebrtcDecodingConfig();
  const MediaEncodingConfiguration* kEncodingConfig =
      CreateWebrtcEncodingConfig();
  const media::mojom::blink::WebrtcPredictionFeatures kDecodeFeatures =
      CreateWebrtcFeatures(/*is_decode=*/true);
  const media::mojom::blink::WebrtcPredictionFeatures kEncodeFeatures =
      CreateWebrtcFeatures(/*is_decode=*/false);

  EXPECT_TRUE(kDecodeFeatures.is_decode_stats);
  EXPECT_FALSE(kEncodeFeatures.is_decode_stats);

  EXPECT_TRUE(kDecodingConfig->video()->contentType().Contains("video/VP9"));
  EXPECT_TRUE(kEncodingConfig->video()->contentType().Contains("video/VP9"));
  EXPECT_EQ(static_cast<media::VideoCodecProfile>(kDecodeFeatures.profile),
            media::VP9PROFILE_PROFILE0);
  EXPECT_EQ(static_cast<media::VideoCodecProfile>(kEncodeFeatures.profile),
            media::VP9PROFILE_PROFILE0);
  EXPECT_EQ(kCodecProfile, media::VP9PROFILE_PROFILE0);

  EXPECT_EQ(
      kDecodingConfig->video()->width() * kDecodingConfig->video()->height(),
      static_cast<uint32_t>(kDecodeFeatures.video_pixels));
  EXPECT_EQ(
      kEncodingConfig->video()->width() * kEncodingConfig->video()->height(),
      static_cast<uint32_t>(kEncodeFeatures.video_pixels));
}

// Test smoothness predictions from DB (WebrtcPerfHistoryService).
TEST(MediaCapabilitiesTests, WebrtcDecodingBasicVideo) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));
  const auto* kDecodingConfig = CreateWebrtcDecodingConfig();
  const media::mojom::blink::WebrtcPredictionFeatures kFeatures =
      CreateWebrtcFeatures(/*is_decode=*/true);

  // WebrtcPerfHistoryService should be queried for smoothness. Signal
  // smooth=true.
  EXPECT_CALL(*context.GetWebrtcPerfHistoryService(), GetPerfInfo(_, _, _))
      .WillOnce(WebrtcDbCallback(kFeatures, kFramerate, /*is_smooth=*/true));
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_TRUE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());

  // Verify DB call was made.
  testing::Mock::VerifyAndClearExpectations(
      context.GetWebrtcPerfHistoryService());

  // Repeat test with smooth=false.
  EXPECT_CALL(*context.GetWebrtcPerfHistoryService(), GetPerfInfo(_, _, _))
      .WillOnce(WebrtcDbCallback(kFeatures, kFramerate, /*is_smooth=*/false));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, WebrtcDecodingUnsupportedVideo) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));

  const MediaDecodingConfiguration* kDecodingConfig =
      CreateVideoConfig<MediaDecodingConfiguration>("video/FooCodec", "webrtc");

  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, WebrtcDecodingSpatialScalability) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));

  auto* decoding_config = CreateWebrtcDecodingConfig();
  auto* video_config = decoding_config->getVideoOr(nullptr);
  video_config->setSpatialScalability(false);
  const media::mojom::blink::WebrtcPredictionFeatures kFeatures =
      CreateWebrtcFeatures(/*is_decode=*/true);

  // WebrtcPerfHistoryService should be queried for smoothness. Signal
  // smooth=true.
  EXPECT_CALL(*context.GetWebrtcPerfHistoryService(), GetPerfInfo(_, _, _))
      .WillOnce(WebrtcDbCallback(kFeatures, kFramerate, /*is_smooth=*/true));
  MediaCapabilitiesInfo* info = DecodingInfo(decoding_config, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_TRUE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());

  // Verify DB call was made.
  testing::Mock::VerifyAndClearExpectations(
      context.GetWebrtcPerfHistoryService());

  // Repeat test with spatialScalability=true.
  video_config->setSpatialScalability(true);
  EXPECT_CALL(*context.GetWebrtcPerfHistoryService(), GetPerfInfo(_, _, _))
      .WillOnce(WebrtcDbCallback(kFeatures, kFramerate, /*is_smooth=*/false));
  info = DecodingInfo(decoding_config, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
}

// WebRTC encodingInfo tests.
TEST(MediaCapabilitiesTests, WebrtcEncodingBasicAudio) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));

  const MediaEncodingConfiguration* kEncodingConfig =
      CreateWebrtcAudioEncodingConfig();
  MediaCapabilitiesInfo* info = EncodingInfo(kEncodingConfig, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, WebrtcEncodingUnsupportedAudio) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));
  const MediaEncodingConfiguration* kEncodingConfig =
      CreateAudioConfig<MediaEncodingConfiguration>("audio/FooCodec", "webrtc");
  MediaCapabilitiesInfo* info = EncodingInfo(kEncodingConfig, &context);
  EXPECT_FALSE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
}

// Test smoothness predictions from DB (WebrtcPerfHistoryService).
TEST(MediaCapabilitiesTests, WebrtcEncodingBasicVideo) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));
  const auto* kEncodingConfig = CreateWebrtcEncodingConfig();
  const media::mojom::blink::WebrtcPredictionFeatures kFeatures =
      CreateWebrtcFeatures(/*is_decode=*/false);

  // WebrtcPerfHistoryService should be queried for smoothness. Signal
  // smooth=true.
  EXPECT_CALL(*context.GetWebrtcPerfHistoryService(), GetPerfInfo(_, _, _))
      .WillOnce(WebrtcDbCallback(kFeatures, kFramerate, /*is_smooth=*/true));
  MediaCapabilitiesInfo* info = EncodingInfo(kEncodingConfig, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_TRUE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());

  // Verify DB call was made.
  testing::Mock::VerifyAndClearExpectations(
      context.GetWebrtcPerfHistoryService());

  // Repeat test with smooth=false.
  EXPECT_CALL(*context.GetWebrtcPerfHistoryService(), GetPerfInfo(_, _, _))
      .WillOnce(WebrtcDbCallback(kFeatures, kFramerate, /*is_smooth=*/false));
  info = EncodingInfo(kEncodingConfig, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, WebrtcEncodingUnsupportedVideo) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));

  const MediaEncodingConfiguration* kEncodingConfig =
      CreateVideoConfig<MediaEncodingConfiguration>("video/FooCodec", "webrtc");

  MediaCapabilitiesInfo* info = EncodingInfo(kEncodingConfig, &context);
  EXPECT_FALSE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, WebrtcEncodingScalabilityMode) {
  test::TaskEnvironment task_environment;
  MediaCapabilitiesTestContext context;
  EXPECT_CALL(context.GetMockPlatform(), GetGpuFactories())
      .Times(testing::AtMost(1));
  auto* encoding_config = CreateWebrtcEncodingConfig();
  auto* video_config = encoding_config->getVideoOr(nullptr);
  video_config->setScalabilityMode("L3T3_KEY");
  const media::mojom::blink::WebrtcPredictionFeatures kFeatures =
      CreateWebrtcFeatures(/*is_decode=*/false);

  // WebrtcPerfHistoryService should be queried for smoothness. Signal
  // smooth=true.
  EXPECT_CALL(*context.GetWebrtcPerfHistoryService(), GetPerfInfo(_, _, _))
      .WillOnce(WebrtcDbCallback(kFeatures, kFramerate, /*is_smooth=*/true));
  MediaCapabilitiesInfo* info = EncodingInfo(encoding_config, &context);
  EXPECT_TRUE(info->supported());
  EXPECT_TRUE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());

  // Verify DB call was made.
  testing::Mock::VerifyAndClearExpectations(
      context.GetWebrtcPerfHistoryService());

  // Repeat with unsupported mode.
  video_config->setScalabilityMode("L3T2_Foo");
  info = EncodingInfo(encoding_config, &context);
  EXPECT_FALSE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, WebrtcDecodePowerEfficientIsSmooth) {
  test::TaskEnvironment task_environment;
  // Set up a custom decoding info handler with a GPU factory that returns
  // supported and powerEfficient.
  MediaCapabilitiesTestContext context;
  auto mock_gpu_factories =
      std::make_unique<media::MockGpuVideoAcceleratorFactories>(nullptr);
  WebrtcDecodingInfoHandler decoding_info_handler(
      blink::CreateWebrtcVideoDecoderFactory(
          mock_gpu_factories.get(),
          Platform::Current()->GetRenderingColorSpace(), base::DoNothing()),
      blink::CreateWebrtcAudioDecoderFactory());

  context.GetMediaCapabilities()->set_webrtc_decoding_info_handler_for_test(
      &decoding_info_handler);

  EXPECT_CALL(*mock_gpu_factories, IsDecoderSupportKnown())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_gpu_factories, IsDecoderConfigSupported(_))
      .WillOnce(Return(media::GpuVideoAcceleratorFactories::Supported::kTrue));

  const auto* kDecodingConfig = CreateWebrtcDecodingConfig();
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  // Expect that powerEfficient==true implies that smooth==true without querying
  // perf history.
  EXPECT_TRUE(info->supported());
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, WebrtcDecodeOverridePowerEfficientIsSmooth) {
  test::TaskEnvironment task_environment;
  // Override the default behavior using a field trial. Query smooth from perf
  // history regardless the value of powerEfficient.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      // Enabled features w/ parameters
      {{media::kWebrtcMediaCapabilitiesParameters,
        {{MediaCapabilities::kWebrtcDecodeSmoothIfPowerEfficientParamName,
          "false"}}}},
      // Disabled features.
      {});

  // Set up a custom decoding info handler with a GPU factory that returns
  // supported and powerEfficient.
  MediaCapabilitiesTestContext context;
  media::MockGpuVideoAcceleratorFactories mock_gpu_factories(nullptr);
  WebrtcDecodingInfoHandler decoding_info_handler(
      blink::CreateWebrtcVideoDecoderFactory(
          &mock_gpu_factories, Platform::Current()->GetRenderingColorSpace(),
          base::DoNothing()),
      blink::CreateWebrtcAudioDecoderFactory());
  context.GetMediaCapabilities()->set_webrtc_decoding_info_handler_for_test(
      &decoding_info_handler);

  EXPECT_CALL(mock_gpu_factories, IsDecoderSupportKnown())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_gpu_factories, IsDecoderConfigSupported(_))
      .WillOnce(Return(media::GpuVideoAcceleratorFactories::Supported::kTrue));

  const auto* kDecodingConfig = CreateWebrtcDecodingConfig();
  media::mojom::blink::WebrtcPredictionFeatures expected_features =
      CreateWebrtcFeatures(/*is_decode=*/true);
  expected_features.hardware_accelerated = true;

  EXPECT_CALL(*context.GetWebrtcPerfHistoryService(), GetPerfInfo(_, _, _))
      .WillOnce(
          WebrtcDbCallback(expected_features, kFramerate, /*is_smooth=*/false));
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  // Expect powerEfficient is true but smooth returned from perf history is
  // false.
  EXPECT_TRUE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
}

TEST(MediaCapabilitiesTests, WebrtcEncodePowerEfficientIsSmooth) {
  test::TaskEnvironment task_environment;
  // Set up a custom decoding info handler with a GPU factory that returns
  // supported and powerEfficient.
  MediaCapabilitiesTestContext context;
  media::MockGpuVideoAcceleratorFactories mock_gpu_factories(nullptr);

  auto video_encoder_factory =
      std::make_unique<RTCVideoEncoderFactory>(&mock_gpu_factories, nullptr);
  // Ensure all the profiles in our mock GPU factory are allowed.
  video_encoder_factory->clear_disabled_profiles_for_testing();

  WebrtcEncodingInfoHandler encoding_info_handler(
      std::move(video_encoder_factory),
      blink::CreateWebrtcAudioEncoderFactory());
  context.GetMediaCapabilities()->set_webrtc_encoding_info_handler_for_test(
      &encoding_info_handler);

  EXPECT_CALL(mock_gpu_factories, IsEncoderSupportKnown())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_gpu_factories, GetVideoEncodeAcceleratorSupportedProfiles())
      .WillOnce(Return(media::VideoEncodeAccelerator::SupportedProfiles{
          {media::VP9PROFILE_PROFILE0, gfx::Size(kWidth, kHeight)}}));

  const auto* kEncodingConfig = CreateWebrtcEncodingConfig();
  MediaCapabilitiesInfo* info = EncodingInfo(kEncodingConfig, &context);
  // Expect that powerEfficient==true implies that smooth==true without querying
  // perf history.
  EXPECT_TRUE(info->supported());
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());

  // RTCVideoEncoderFactory destroys MojoVideoEncoderMetricsProvider on the
  // task runner of GpuVideoAcceleratorFactories.
  EXPECT_CALL(mock_gpu_factories, GetTaskRunner())
      .WillOnce(Return(base::SequencedTaskRunner::GetCurrentDefault()));
}

TEST(MediaCapabilitiesTests, WebrtcEncodeOverridePowerEfficientIsSmooth) {
  test::TaskEnvironment task_environment;
  // Override the default behavior using a field trial. Query smooth from perf
  // history regardless the value of powerEfficient.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      // Enabled features w/ parameters
      {{media::kWebrtcMediaCapabilitiesParameters,
        {{MediaCapabilities::kWebrtcEncodeSmoothIfPowerEfficientParamName,
          "false"}}}},
      // Disabled features.
      {});

  // Set up a custom decoding info handler with a GPU factory that returns
  // supported and powerEfficient.
  MediaCapabilitiesTestContext context;
  media::MockGpuVideoAcceleratorFactories mock_gpu_factories(nullptr);

  auto video_encoder_factory =
      std::make_unique<RTCVideoEncoderFactory>(&mock_gpu_factories, nullptr);
  // Ensure all the profiles in our mock GPU factory are allowed.
  video_encoder_factory->clear_disabled_profiles_for_testing();

  WebrtcEncodingInfoHandler encoding_info_handler(
      std::move(video_encoder_factory),
      blink::CreateWebrtcAudioEncoderFactory());
  context.GetMediaCapabilities()->set_webrtc_encoding_info_handler_for_test(
      &encoding_info_handler);

  EXPECT_CALL(mock_gpu_factories, IsEncoderSupportKnown())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_gpu_factories, GetVideoEncodeAcceleratorSupportedProfiles())
      .WillOnce(Return(media::VideoEncodeAccelerator::SupportedProfiles{
          {media::VP9PROFILE_PROFILE0, gfx::Size(kWidth, kHeight)}}));

  const auto* kEncodingConfig = CreateWebrtcEncodingConfig();
  media::mojom::blink::WebrtcPredictionFeatures expected_features =
      CreateWebrtcFeatures(/*is_decode=*/false);
  expected_features.hardware_accelerated = true;

  EXPECT_CALL(*context.GetWebrtcPerfHistoryService(), GetPerfInfo(_, _, _))
      .WillOnce(
          WebrtcDbCallback(expected_features, kFramerate, /*is_smooth=*/false));
  MediaCapabilitiesInfo* info = EncodingInfo(kEncodingConfig, &context);
  // Expect powerEfficient is true but smooth returned from perf history is
  // false.
  EXPECT_TRUE(info->supported());
  EXPECT_FALSE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());

  // RTCVideoEncoderFactory destroys MojoVideoEncoderMetricsProvider on the
  // task runner of GpuVideoAcceleratorFactories.
  EXPECT_CALL(mock_gpu_factories, GetTaskRunner())
      .WillOnce(Return(base::SequencedTaskRunner::GetCurrentDefault()));
}

}  // namespace blink
