// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_context.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "media/base/output_device_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_context_options.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

const char* const kAutoplayMetric = "WebAudio.Autoplay";
const char* const kAutoplayCrossOriginMetric = "WebAudio.Autoplay.CrossOrigin";

class MockWebAudioDeviceForAutoplayTest : public WebAudioDevice {
 public:
  explicit MockWebAudioDeviceForAutoplayTest(double sample_rate,
                                             int frames_per_buffer)
      : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}
  ~MockWebAudioDeviceForAutoplayTest() override = default;

  void Start() override {}
  void Stop() override {}
  void Pause() override {}
  void Resume() override {}
  double SampleRate() override { return sample_rate_; }
  int FramesPerBuffer() override { return frames_per_buffer_; }
  int MaxChannelCount() override { return 2; }
  void SetDetectSilence(bool detect_silence) override {}
  media::OutputDeviceStatus MaybeCreateSinkAndGetStatus() override {
    // In this test, we assume the sink creation always succeeds.
    return media::OUTPUT_DEVICE_STATUS_OK;
  }

 private:
  double sample_rate_;
  int frames_per_buffer_;
};

class AudioContextAutoplayTestPlatform : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      const WebAudioSinkDescriptor& sink_descriptor,
      unsigned number_of_output_channels,
      const WebAudioLatencyHint& latency_hint,
      media::AudioRendererSink::RenderCallback*) override {
    return std::make_unique<MockWebAudioDeviceForAutoplayTest>(
        AudioHardwareSampleRate(), AudioHardwareBufferSize());
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 128; }
};

}  // namespace

class AudioContextAutoplayTest
    : public testing::TestWithParam<AutoplayPolicy::Type> {
 protected:
  using AutoplayStatus = AudioContext::AutoplayStatus;

  void SetUp() override {
    helper_.Initialize();
    frame_test_helpers::LoadHTMLString(helper_.LocalMainFrame(),
                                       "<iframe></iframe>",
                                       WebURL(KURL("https://example.com")));
    frame_test_helpers::LoadHTMLString(
        To<WebLocalFrameImpl>(helper_.LocalMainFrame()->FirstChild()), "",
        WebURL(KURL("https://cross-origin.com")));
    GetWindow().GetFrame()->GetSettings()->SetAutoplayPolicy(GetParam());
    ChildWindow().GetFrame()->GetSettings()->SetAutoplayPolicy(GetParam());

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override { MemoryCache::Get()->EvictResources(); }

  LocalDOMWindow& GetWindow() {
    return *helper_.LocalMainFrame()->GetFrame()->DomWindow();
  }

  LocalDOMWindow& ChildWindow() {
    return *To<WebLocalFrameImpl>(helper_.LocalMainFrame()->FirstChild())
                ->GetFrame()
                ->DomWindow();
  }

  ScriptState* GetScriptStateFrom(const LocalDOMWindow& window) {
    return ToScriptStateForMainWorld(window.GetFrame());
  }

  void RejectPendingResolvers(AudioContext* audio_context) {
    audio_context->RejectPendingResolvers();
  }

  void RecordAutoplayStatus(AudioContext* audio_context) {
    audio_context->RecordAutoplayMetrics();
  }

  base::HistogramTester* GetHistogramTester() {
    return histogram_tester_.get();
  }

 private:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<AudioContextAutoplayTestPlatform> platform_;
  frame_test_helpers::WebViewHelper helper_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Creates an AudioContext without a gesture inside a x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CreateNoGesture_Child) {
  AudioContext* audio_context = AudioContext::Create(
      &ChildWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kFailed), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, static_cast<int>(AutoplayStatus::kFailed),
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext without a gesture inside a main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CreateNoGesture_Main) {
  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kFailed), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then call resume without a gesture in a x-origin
// child frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_CallResumeNoGesture_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildWindow()));

  AudioContext* audio_context = AudioContext::Create(
      &ChildWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  audio_context->resumeContext(GetScriptStateFrom(ChildWindow()),
                               ASSERT_NO_EXCEPTION);
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kFailed), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, static_cast<int>(AutoplayStatus::kFailed),
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then call resume without a gesture in a main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CallResumeNoGesture_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetWindow()));

  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  audio_context->resumeContext(GetScriptStateFrom(ChildWindow()),
                               ASSERT_NO_EXCEPTION);
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kFailed), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext with a user gesture inside a x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CreateGesture_Child) {
  LocalFrame::NotifyUserActivation(
      ChildWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);

  AudioContext* audio_context = AudioContext::Create(
      &ChildWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric,
          static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext with a user gesture inside a main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CreateGesture_Main) {
  LocalFrame::NotifyUserActivation(
      GetWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);

  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls resume with a user gesture inside a
// x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CallResumeGesture_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildWindow()));

  AudioContext* audio_context = AudioContext::Create(
      &ChildWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      ChildWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);

  audio_context->resumeContext(GetScriptStateFrom(ChildWindow()),
                               ASSERT_NO_EXCEPTION);
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric,
          static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls resume with a user gesture inside a main
// frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CallResumeGesture_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetWindow()));

  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      GetWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);

  audio_context->resumeContext(GetScriptStateFrom(GetWindow()),
                               ASSERT_NO_EXCEPTION);
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture inside a
// x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_NodeStartNoGesture_Child) {
  AudioContext* audio_context = AudioContext::Create(
      &ChildWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  audio_context->NotifySourceNodeStart();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kFailed), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, static_cast<int>(AutoplayStatus::kFailed),
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture inside a
// main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_NodeStartNoGesture_Main) {
  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  audio_context->NotifySourceNodeStart();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kFailed), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture inside a
// x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_NodeStartGesture_Child) {
  AudioContext* audio_context = AudioContext::Create(
      &ChildWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      ChildWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);
  audio_context->NotifySourceNodeStart();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric,
          static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture inside a
// main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_NodeStartGesture_Main) {
  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      GetWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);
  audio_context->NotifySourceNodeStart();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture and
// finally allows the AudioContext to produce sound inside x-origin child frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_NodeStartNoGestureThenSuccess_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildWindow()));

  AudioContext* audio_context = AudioContext::Create(
      &ChildWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  audio_context->NotifySourceNodeStart();

  LocalFrame::NotifyUserActivation(
      ChildWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);
  audio_context->resumeContext(GetScriptStateFrom(ChildWindow()),
                               ASSERT_NO_EXCEPTION);
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric,
          static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture and
// finally allows the AudioContext to produce sound inside a main frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_NodeStartNoGestureThenSuccess_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetWindow()));

  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  audio_context->NotifySourceNodeStart();

  LocalFrame::NotifyUserActivation(
      GetWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);
  audio_context->resumeContext(GetScriptStateFrom(GetWindow()),
                               ASSERT_NO_EXCEPTION);
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture and
// finally allows the AudioContext to produce sound inside x-origin child frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_NodeStartGestureThenSucces_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildWindow()));

  AudioContext* audio_context = AudioContext::Create(
      &ChildWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      ChildWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);
  audio_context->NotifySourceNodeStart();
  audio_context->resumeContext(GetScriptStateFrom(ChildWindow()),
                               ASSERT_NO_EXCEPTION);
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric,
          static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture and
// finally allows the AudioContext to produce sound inside a main frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_NodeStartGestureThenSucces_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetWindow()));

  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      GetWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);
  audio_context->NotifySourceNodeStart();
  audio_context->resumeContext(GetScriptStateFrom(GetWindow()),
                               ASSERT_NO_EXCEPTION);
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Attempts to autoplay an AudioContext in a x-origin child frame when the
// document previous received a user gesture.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_DocumentReceivedGesture_Child) {
  LocalFrame::NotifyUserActivation(
      ChildWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);

  AudioContext* audio_context = AudioContext::Create(
      &ChildWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric,
          static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric,
          static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Attempts to autoplay an AudioContext in a main child frame when the
// document previous received a user gesture.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_DocumentReceivedGesture_Main) {
  LocalFrame::NotifyUserActivation(
      ChildWindow().GetFrame(), mojom::UserActivationNotificationType::kTest);

  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Attempts to autoplay an AudioContext in a main child frame when the
// document received a user gesture before navigation.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_DocumentReceivedGesture_BeforeNavigation) {
  GetWindow().GetFrame()->SetHadStickyUserActivationBeforeNavigation(true);

  AudioContext* audio_context = AudioContext::Create(
      &GetWindow(), AudioContextOptions::Create(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, static_cast<int>(AutoplayStatus::kSucceeded), 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    AudioContextAutoplayTest,
    AudioContextAutoplayTest,
    testing::Values(AutoplayPolicy::Type::kNoUserGestureRequired,
                    AutoplayPolicy::Type::kUserGestureRequired,
                    AutoplayPolicy::Type::kDocumentUserActivationRequired));

}  // namespace blink
