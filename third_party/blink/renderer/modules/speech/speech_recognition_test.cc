// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_recognition.h"

#include <optional>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition_result.mojom-blink.h"
#include "media/mojo/mojom/speech_recognizer.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_speech_recognition_options.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

using AvailabilityStatus = media::mojom::blink::AvailabilityStatus;

constexpr char kDownloadActivationOutcomeHistogram[] =
    "Accessibility.WebSpeech.DownloadActivationOutcome";

// LINT.IfChange(WebSpeechDownloadActivationOutcome)
constexpr int kOutcomeBlockedNoTransientActivation = 0;
constexpr int kOutcomeBlockedNoStickyActivation = 1;
constexpr int kOutcomeRelaxed = 2;
constexpr int kOutcomeAllowedNoStickyActivation = 3;
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:WebSpeechDownloadActivationOutcome,
// //third_party/blink/renderer/modules/speech/speech_recognition.cc:WebSpeechDownloadActivationOutcome)

constexpr char kTransientActivationMessage[] =
    "NotAllowedError: Requires handling a user gesture when availability is "
    "\"downloadable\".";
constexpr char kStickyActivationMessage[] =
    "NotAllowedError: Requires sticky activation when availability is "
    "\"downloadable\".";

// Fake OnDeviceSpeechRecognition that reports a fixed availability status.
class FakeOnDeviceSpeechRecognition
    : public media::mojom::blink::OnDeviceSpeechRecognition {
 public:
  explicit FakeOnDeviceSpeechRecognition(AvailabilityStatus status)
      : status_(status) {}

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(
        this,
        mojo::PendingReceiver<media::mojom::blink::OnDeviceSpeechRecognition>(
            std::move(handle)));
  }

  bool install_called() const { return install_called_; }
  const Vector<String>& install_languages() const { return install_languages_; }

  // media::mojom::blink::OnDeviceSpeechRecognition:
  void Available(const Vector<String>& languages,
                 media::mojom::blink::SpeechRecognitionQuality quality,
                 AvailableCallback callback) override {
    std::move(callback).Run(status_);
  }

  void Install(const Vector<String>& languages,
               media::mojom::blink::SpeechRecognitionQuality quality,
               InstallCallback callback) override {
    install_called_ = true;
    install_languages_ = languages;
    std::move(callback).Run(true);
  }

 private:
  const AvailabilityStatus status_;
  bool install_called_ = false;
  Vector<String> install_languages_;
  mojo::ReceiverSet<media::mojom::blink::OnDeviceSpeechRecognition> receivers_;
};

// Registers `fake` with the browser interface broker and unregisters on
// destruction.
class ScopedFakeOnDeviceSpeechRecognition {
  STACK_ALLOCATED();

 public:
  ScopedFakeOnDeviceSpeechRecognition(V8TestingScope& scope,
                                      FakeOnDeviceSpeechRecognition* fake)
      : broker_(&scope.GetWindow().GetBrowserInterfaceBroker()) {
    CHECK(broker_->SetBinderForTesting(
        media::mojom::blink::OnDeviceSpeechRecognition::Name_,
        BindRepeating(&FakeOnDeviceSpeechRecognition::Bind, Unretained(fake))))
        << "A binder for OnDeviceSpeechRecognition is already registered; "
           "SetBinderForTesting() does not overwrite existing entries.";
  }

  ScopedFakeOnDeviceSpeechRecognition(
      const ScopedFakeOnDeviceSpeechRecognition&) = delete;
  ScopedFakeOnDeviceSpeechRecognition& operator=(
      const ScopedFakeOnDeviceSpeechRecognition&) = delete;

  ~ScopedFakeOnDeviceSpeechRecognition() {
    broker_->SetBinderForTesting(
        media::mojom::blink::OnDeviceSpeechRecognition::Name_, {});
  }

 private:
  const BrowserInterfaceBrokerProxy* broker_;
};

// Captures the `timeStamp` of each dispatched `result` event.
class ResultEventRecorder final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    ++count_;
    last_time_stamp_ = event->PlatformTimeStamp();
  }

  int count() const { return count_; }
  base::TimeTicks last_time_stamp() const { return last_time_stamp_; }

 private:
  int count_ = 0;
  base::TimeTicks last_time_stamp_;
};

media::mojom::blink::WebSpeechRecognitionResultPtr MakeResult(
    const String& utterance,
    bool is_provisional,
    std::optional<base::TimeDelta> audio_start_time,
    std::optional<base::TimeDelta> audio_end_time) {
  auto result = media::mojom::blink::WebSpeechRecognitionResult::New();
  result->is_provisional = is_provisional;
  result->audio_start_time = audio_start_time;
  result->audio_end_time = audio_end_time;

  auto hypothesis = media::mojom::blink::SpeechRecognitionHypothesis::New();
  hypothesis->utterance = utterance;
  hypothesis->confidence = 1.0;
  result->hypotheses.push_back(std::move(hypothesis));

  return result;
}

}  // namespace


class SpeechRecognitionTest : public PageTestBase {
 public:
  void SetUp() override { PageTestBase::SetUp(); }
};

TEST_F(SpeechRecognitionTest, MediaStreamTrackAudioSinkCleanup) {
  ScopedMediaStreamTrackWebSpeechForTest media_stream_track_web_speech(true);

  V8TestingScope scope;
  LocalDOMWindow* window = &scope.GetWindow();

  auto* track = MakeGarbageCollected<MockMediaStreamTrack>();
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::kTypeAudio, "name", false,
      /*platform_source=*/nullptr, MediaStreamSource::kReadyStateLive);
  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      source, std::make_unique<MediaStreamAudioTrack>(true));
  track->SetComponent(component);
  track->SetReadyState(MediaStreamSource::kReadyStateLive);

  auto* speech_recognition = SpeechRecognition::Create(window);

  EXPECT_CALL(*track, RegisterSink(testing::_)).Times(1);
  EXPECT_CALL(*track, UnregisterSink(testing::_)).Times(1);

  speech_recognition->start(track, scope.GetExceptionState());

  speech_recognition->abort();

  testing::Mock::VerifyAndClearExpectations(track);
  testing::Mock::AllowLeak(track);
}

// Tests user activation requirements for SpeechRecognition.install().
class SpeechRecognitionInstallTest : public testing::Test {
 protected:
  SpeechRecognitionOptions* CreateOptions(V8TestingScope& scope) {
    auto* options = SpeechRecognitionOptions::Create(scope.GetIsolate());
    options->setLangs(Vector<String>{"en-US"});
    options->setProcessLocally(true);
    return options;
  }

  // Grants sticky activation only.
  void GrantStickyActivationOnly(V8TestingScope& scope) {
    LocalFrame::NotifyUserActivation(
        &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
    LocalFrame::ConsumeTransientUserActivation(&scope.GetFrame());
    ASSERT_TRUE(scope.GetFrame().HasStickyUserActivation());
    ASSERT_FALSE(LocalFrame::HasTransientUserActivation(&scope.GetFrame()));
  }

  test::TaskEnvironment task_environment_;
};

// The relaxed path still requires sticky activation.
TEST_F(SpeechRecognitionInstallTest, RelaxedRequiresStickyActivation) {
  FakeOnDeviceSpeechRecognition fake(
      AvailabilityStatus::kDownloadableWithoutTransientUserActivation);
  V8TestingScope scope(KURL("https://example.com/"));
  base::HistogramTester histogram_tester;
  ScopedFakeOnDeviceSpeechRecognition scoped_fake(scope, &fake);

  ASSERT_FALSE(scope.GetFrame().HasStickyUserActivation());

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      SpeechRecognition::install(scope.GetScriptState(), CreateOptions(scope),
                                 scope.GetExceptionState()),
      &scope.GetExceptionState());
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
  EXPECT_EQ(kStickyActivationMessage, tester.ValueAsString());
  EXPECT_FALSE(fake.install_called());
  histogram_tester.ExpectUniqueSample(kDownloadActivationOutcomeHistogram,
                                      kOutcomeBlockedNoStickyActivation, 1);
}

// Install succeeds with sticky activation on the relaxed path.
TEST_F(SpeechRecognitionInstallTest, RelaxedAllowedWithStickyActivation) {
  FakeOnDeviceSpeechRecognition fake(
      AvailabilityStatus::kDownloadableWithoutTransientUserActivation);
  V8TestingScope scope(KURL("https://example.com/"));
  base::HistogramTester histogram_tester;
  ScopedFakeOnDeviceSpeechRecognition scoped_fake(scope, &fake);

  ASSERT_NO_FATAL_FAILURE(GrantStickyActivationOnly(scope));

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      SpeechRecognition::install(scope.GetScriptState(), CreateOptions(scope),
                                 scope.GetExceptionState()),
      &scope.GetExceptionState());
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(fake.install_called());
  EXPECT_EQ(Vector<String>{"en-US"}, fake.install_languages());
  histogram_tester.ExpectUniqueSample(kDownloadActivationOutcomeHistogram,
                                      kOutcomeRelaxed, 1);
}

// Kill switch restores requiring no activation on the relaxed path.
TEST_F(SpeechRecognitionInstallTest, RelaxedAllowedWithKillSwitchDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      media::kOnDeviceWebSpeechRequiresStickyActivation);

  FakeOnDeviceSpeechRecognition fake(
      AvailabilityStatus::kDownloadableWithoutTransientUserActivation);
  V8TestingScope scope(KURL("https://example.com/"));
  base::HistogramTester histogram_tester;
  ScopedFakeOnDeviceSpeechRecognition scoped_fake(scope, &fake);

  ASSERT_FALSE(scope.GetFrame().HasStickyUserActivation());

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      SpeechRecognition::install(scope.GetScriptState(), CreateOptions(scope),
                                 scope.GetExceptionState()),
      &scope.GetExceptionState());
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(fake.install_called());
  // Records missing sticky activation even when requirement is disabled.
  histogram_tester.ExpectUniqueSample(kDownloadActivationOutcomeHistogram,
                                      kOutcomeAllowedNoStickyActivation, 1);
}

// Relaxed path does not consume transient activation.
TEST_F(SpeechRecognitionInstallTest, RelaxedDoesNotConsumeTransientActivation) {
  FakeOnDeviceSpeechRecognition fake(
      AvailabilityStatus::kDownloadableWithoutTransientUserActivation);
  V8TestingScope scope(KURL("https://example.com/"));
  base::HistogramTester histogram_tester;
  ScopedFakeOnDeviceSpeechRecognition scoped_fake(scope, &fake);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      SpeechRecognition::install(scope.GetScriptState(), CreateOptions(scope),
                                 scope.GetExceptionState()),
      &scope.GetExceptionState());
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(fake.install_called());
  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(&scope.GetFrame()));
  // Nothing recorded when transient activation is present.
  histogram_tester.ExpectTotalCount(kDownloadActivationOutcomeHistogram, 0);
}

// Unrelaxed downloads require transient activation; sticky is insufficient.
TEST_F(SpeechRecognitionInstallTest, DownloadableRequiresTransientActivation) {
  FakeOnDeviceSpeechRecognition fake(AvailabilityStatus::kDownloadable);
  V8TestingScope scope(KURL("https://example.com/"));
  base::HistogramTester histogram_tester;
  ScopedFakeOnDeviceSpeechRecognition scoped_fake(scope, &fake);

  ASSERT_NO_FATAL_FAILURE(GrantStickyActivationOnly(scope));

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      SpeechRecognition::install(scope.GetScriptState(), CreateOptions(scope),
                                 scope.GetExceptionState()),
      &scope.GetExceptionState());
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());
  EXPECT_EQ(kTransientActivationMessage, tester.ValueAsString());
  EXPECT_FALSE(fake.install_called());
  histogram_tester.ExpectUniqueSample(kDownloadActivationOutcomeHistogram,
                                      kOutcomeBlockedNoTransientActivation, 1);
}

// Unrelaxed path consumes transient activation.
TEST_F(SpeechRecognitionInstallTest, DownloadableConsumesTransientActivation) {
  FakeOnDeviceSpeechRecognition fake(AvailabilityStatus::kDownloadable);
  V8TestingScope scope(KURL("https://example.com/"));
  base::HistogramTester histogram_tester;
  ScopedFakeOnDeviceSpeechRecognition scoped_fake(scope, &fake);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      SpeechRecognition::install(scope.GetScriptState(), CreateOptions(scope),
                                 scope.GetExceptionState()),
      &scope.GetExceptionState());
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(fake.install_called());
  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&scope.GetFrame()));
  histogram_tester.ExpectTotalCount(kDownloadActivationOutcomeHistogram, 0);
}

// An already installed language pack needs no activation at all.
TEST_F(SpeechRecognitionInstallTest, AvailableRequiresNoActivation) {
  FakeOnDeviceSpeechRecognition fake(AvailabilityStatus::kAvailable);
  V8TestingScope scope(KURL("https://example.com/"));
  base::HistogramTester histogram_tester;
  ScopedFakeOnDeviceSpeechRecognition scoped_fake(scope, &fake);

  ASSERT_FALSE(scope.GetFrame().HasStickyUserActivation());

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      SpeechRecognition::install(scope.GetScriptState(), CreateOptions(scope),
                                 scope.GetExceptionState()),
      &scope.GetExceptionState());
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(fake.install_called());
  histogram_tester.ExpectTotalCount(kDownloadActivationOutcomeHistogram, 0);
}

// An unsupported language resolves with false without attempting an install.
TEST_F(SpeechRecognitionInstallTest, UnavailableResolvesFalse) {
  FakeOnDeviceSpeechRecognition fake(AvailabilityStatus::kUnavailable);
  V8TestingScope scope(KURL("https://example.com/"));
  base::HistogramTester histogram_tester;
  ScopedFakeOnDeviceSpeechRecognition scoped_fake(scope, &fake);

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      SpeechRecognition::install(scope.GetScriptState(), CreateOptions(scope),
                                 scope.GetExceptionState()),
      &scope.GetExceptionState());
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_EQ("false", tester.ValueAsString());
  EXPECT_FALSE(fake.install_called());
  histogram_tester.ExpectTotalCount(kDownloadActivationOutcomeHistogram, 0);
}

TEST_F(SpeechRecognitionTest, ResultEventTimeStampUsesRecognitionEndTime) {
  V8TestingScope scope;
  auto* speech_recognition = SpeechRecognition::Create(&scope.GetWindow());

  auto* recorder = MakeGarbageCollected<ResultEventRecorder>();
  speech_recognition->addEventListener(event_type_names::kResult, recorder);

  // Bracket the audio start so we know the time origin to within a window.
  const base::TimeTicks before_audio_start = base::TimeTicks::Now();
  speech_recognition->AudioStarted();
  const base::TimeTicks after_audio_start = base::TimeTicks::Now();

  constexpr base::TimeDelta kAudioStartTime = base::Milliseconds(500);
  constexpr base::TimeDelta kAudioEndTime = base::Milliseconds(1500);

  Vector<media::mojom::blink::WebSpeechRecognitionResultPtr> results;
  results.push_back(MakeResult("hello world", /*is_provisional=*/false,
                               kAudioStartTime, kAudioEndTime));
  speech_recognition->ResultRetrieved(std::move(results));

  ASSERT_EQ(recorder->count(), 1);
  // The event is stamped with the audio time origin plus the recognizer's
  // reported end offset, not with the time the event was constructed.
  EXPECT_GE(recorder->last_time_stamp(), before_audio_start + kAudioEndTime);
  EXPECT_LE(recorder->last_time_stamp(), after_audio_start + kAudioEndTime);
}

TEST_F(SpeechRecognitionTest, ResultEventTimeStampUsesLatestEndTime) {
  V8TestingScope scope;
  auto* speech_recognition = SpeechRecognition::Create(&scope.GetWindow());

  auto* recorder = MakeGarbageCollected<ResultEventRecorder>();
  speech_recognition->addEventListener(event_type_names::kResult, recorder);

  const base::TimeTicks before_audio_start = base::TimeTicks::Now();
  speech_recognition->AudioStarted();
  const base::TimeTicks after_audio_start = base::TimeTicks::Now();

  constexpr base::TimeDelta kFinalEndTime = base::Milliseconds(1000);
  constexpr base::TimeDelta kProvisionalEndTime = base::Milliseconds(2500);

  Vector<media::mojom::blink::WebSpeechRecognitionResultPtr> results;
  results.push_back(MakeResult("final", /*is_provisional=*/false,
                               base::Milliseconds(0), kFinalEndTime));
  results.push_back(MakeResult("provisional", /*is_provisional=*/true,
                               kFinalEndTime, kProvisionalEndTime));
  speech_recognition->ResultRetrieved(std::move(results));

  ASSERT_EQ(recorder->count(), 1);
  // The newest audio in the batch determines the event time.
  EXPECT_GE(recorder->last_time_stamp(),
            before_audio_start + kProvisionalEndTime);
  EXPECT_LE(recorder->last_time_stamp(),
            after_audio_start + kProvisionalEndTime);
}

TEST_F(SpeechRecognitionTest, ResultEventTimeStampFallsBackWithoutTimestamps) {
  V8TestingScope scope;
  auto* speech_recognition = SpeechRecognition::Create(&scope.GetWindow());

  auto* recorder = MakeGarbageCollected<ResultEventRecorder>();
  speech_recognition->addEventListener(event_type_names::kResult, recorder);

  speech_recognition->AudioStarted();

  Vector<media::mojom::blink::WebSpeechRecognitionResultPtr> results;
  results.push_back(MakeResult("hello world", /*is_provisional=*/false,
                               /*audio_start_time=*/std::nullopt,
                               /*audio_end_time=*/std::nullopt));

  const base::TimeTicks before_dispatch = base::TimeTicks::Now();
  speech_recognition->ResultRetrieved(std::move(results));
  const base::TimeTicks after_dispatch = base::TimeTicks::Now();

  ASSERT_EQ(recorder->count(), 1);
  // Without recognizer timestamps the event falls back to the dispatch time.
  EXPECT_GE(recorder->last_time_stamp(), before_dispatch);
  EXPECT_LE(recorder->last_time_stamp(), after_dispatch);
}

}  // namespace blink
