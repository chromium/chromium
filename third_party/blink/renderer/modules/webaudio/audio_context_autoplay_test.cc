// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_context.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_audio_device.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_thread.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

const char* const kAutoplayMetric = "WebAudio.Autoplay";
const char* const kAutoplayCrossOriginMetric = "WebAudio.Autoplay.CrossOrigin";

class MockCrossOriginLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  static MockCrossOriginLocalFrameClient* Create(Frame* parent) {
    return new MockCrossOriginLocalFrameClient(parent);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(parent_);
    EmptyLocalFrameClient::Trace(visitor);
  }

  Frame* Parent() const override { return parent_.Get(); }
  Frame* Top() const override { return parent_.Get(); }

 private:
  explicit MockCrossOriginLocalFrameClient(Frame* parent) : parent_(parent) {}

  Member<Frame> parent_;
};

class MockWebAudioDeviceForAutoplayTest : public WebAudioDevice {
 public:
  explicit MockWebAudioDeviceForAutoplayTest(double sample_rate,
                                             int frames_per_buffer)
      : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}
  ~MockWebAudioDeviceForAutoplayTest() override = default;

  void Start() override {}
  void Stop() override {}
  double SampleRate() override { return sample_rate_; }
  int FramesPerBuffer() override { return frames_per_buffer_; }

 private:
  double sample_rate_;
  int frames_per_buffer_;
};

class AudioContextAutoplayTestPlatform : public TestingPlatformSupport {
 public:
  std::unique_ptr<WebAudioDevice> CreateAudioDevice(
      unsigned number_of_input_channels,
      unsigned number_of_channels,
      const WebAudioLatencyHint& latency_hint,
      WebAudioDevice::RenderCallback*,
      const WebString& device_id) override {
    return std::make_unique<MockWebAudioDeviceForAutoplayTest>(
        AudioHardwareSampleRate(), AudioHardwareBufferSize());
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 128; }
};

}  // anonymous namespace

class AudioContextAutoplayTest
    : public testing::TestWithParam<AutoplayPolicy::Type> {
 protected:
  using AutoplayStatus = AudioContext::AutoplayStatus;

  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create();
    dummy_frame_owner_ = DummyFrameOwner::Create();
    GetDocument().UpdateSecurityOrigin(
        SecurityOrigin::Create("https", "example.com", 80));

    CreateChildFrame();

    GetDocument().GetSettings()->SetAutoplayPolicy(GetParam());
    ChildDocument().GetSettings()->SetAutoplayPolicy(GetParam());

    histogram_tester_ = std::make_unique<HistogramTester>();
  }

  void TearDown() override {
    if (child_frame_)
      child_frame_->Detach(FrameDetachType::kRemove);
  }

  void CreateChildFrame() {
    child_frame_ = LocalFrame::Create(
        MockCrossOriginLocalFrameClient::Create(GetDocument().GetFrame()),
        *GetDocument().GetFrame()->GetPage(), dummy_frame_owner_.Get());
    child_frame_->SetView(
        LocalFrameView::Create(*child_frame_, IntSize(500, 500)));
    child_frame_->Init();

    ChildDocument().UpdateSecurityOrigin(
        SecurityOrigin::Create("https", "cross-origin.com", 80));
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  Document& ChildDocument() { return *child_frame_->GetDocument(); }

  ScriptState* GetScriptStateFrom(const Document& document) {
    return ToScriptStateForMainWorld(document.GetFrame());
  }

  void RejectPendingResolvers(AudioContext* audio_context) {
    audio_context->RejectPendingResolvers();
  }

  void RecordAutoplayStatus(AudioContext* audio_context) {
    audio_context->RecordAutoplayMetrics();
  }

  HistogramTester* GetHistogramTester() {
    return histogram_tester_.get();
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<DummyFrameOwner> dummy_frame_owner_;
  Persistent<LocalFrame> child_frame_;
  std::unique_ptr<HistogramTester> histogram_tester_;
  ScopedTestingPlatformSupport<AudioContextAutoplayTestPlatform> platform_;
};

// Creates an AudioContext without a gesture inside a x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CreateNoGesture_Child) {
  AudioContext* audio_context = AudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext without a gesture inside a main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CreateNoGesture_Main) {
  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then call resume without a gesture in a x-origin
// child frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_CallResumeNoGesture_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  AudioContext* audio_context = AudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then call resume without a gesture in a main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CallResumeNoGesture_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetDocument()));

  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext with a user gesture inside a x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CreateGesture_Child) {
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(ChildDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);

  AudioContext* audio_context = AudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext with a user gesture inside a main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CreateGesture_Main) {
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);

  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls resume with a user gesture inside a
// x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CallResumeGesture_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  AudioContext* audio_context = AudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(ChildDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);

  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls resume with a user gesture inside a main
// frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_CallResumeGesture_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetDocument()));

  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);

  audio_context->resumeContext(GetScriptStateFrom(GetDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture inside a
// x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_NodeStartNoGesture_Child) {
  AudioContext* audio_context = AudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->NotifySourceNodeStart();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture inside a
// main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_NodeStartNoGesture_Main) {
  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->NotifySourceNodeStart();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture inside a
// x-origin child frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_NodeStartGesture_Child) {
  AudioContext* audio_context = AudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(ChildDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);
  audio_context->NotifySourceNodeStart();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture inside a
// main frame.
TEST_P(AudioContextAutoplayTest, AutoplayMetrics_NodeStartGesture_Main) {
  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);
  audio_context->NotifySourceNodeStart();
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture and
// finally allows the AudioContext to produce sound inside x-origin child frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_NodeStartNoGestureThenSuccess_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  AudioContext* audio_context = AudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->NotifySourceNodeStart();

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(ChildDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node without a gesture and
// finally allows the AudioContext to produce sound inside a main frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_NodeStartNoGestureThenSuccess_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetDocument()));

  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  audio_context->NotifySourceNodeStart();

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);
  audio_context->resumeContext(GetScriptStateFrom(GetDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture and
// finally allows the AudioContext to produce sound inside x-origin child frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_NodeStartGestureThenSucces_Child) {
  ScriptState::Scope scope(GetScriptStateFrom(ChildDocument()));

  AudioContext* audio_context = AudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(ChildDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);
  audio_context->NotifySourceNodeStart();
  audio_context->resumeContext(GetScriptStateFrom(ChildDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Creates an AudioContext then calls start on a node with a gesture and
// finally allows the AudioContext to produce sound inside a main frame.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_NodeStartGestureThenSucces_Main) {
  ScriptState::Scope scope(GetScriptStateFrom(GetDocument()));

  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);

  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);
  audio_context->NotifySourceNodeStart();
  audio_context->resumeContext(GetScriptStateFrom(GetDocument()));
  RejectPendingResolvers(audio_context);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Attempts to autoplay an AudioContext in a x-origin child frame when the
// document previous received a user gesture.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_DocumentReceivedGesture_Child) {
  LocalFrame::NotifyUserActivation(ChildDocument().GetFrame(),
                                   UserGestureToken::kNewGesture);

  AudioContext* audio_context = AudioContext::Create(
      ChildDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusFailed, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayCrossOriginMetric, AutoplayStatus::kAutoplayStatusSucceeded,
          1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 1);
      break;
  }
}

// Attempts to autoplay an AudioContext in a main child frame when the
// document previous received a user gesture.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_DocumentReceivedGesture_Main) {
  LocalFrame::NotifyUserActivation(ChildDocument().GetFrame(),
                                   UserGestureToken::kNewGesture);

  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

// Attempts to autoplay an AudioContext in a main child frame when the
// document received a user gesture before navigation.
TEST_P(AudioContextAutoplayTest,
       AutoplayMetrics_DocumentReceivedGesture_BeforeNavigation) {
  GetDocument().GetFrame()->SetDocumentHasReceivedUserGestureBeforeNavigation(
      true);

  AudioContext* audio_context = AudioContext::Create(
      GetDocument(), AudioContextOptions(), ASSERT_NO_EXCEPTION);
  RecordAutoplayStatus(audio_context);

  switch (GetParam()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 0);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      GetHistogramTester()->ExpectBucketCount(
          kAutoplayMetric, AutoplayStatus::kAutoplayStatusSucceeded, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayMetric, 1);
      GetHistogramTester()->ExpectTotalCount(kAutoplayCrossOriginMetric, 0);
      break;
  }
}

INSTANTIATE_TEST_CASE_P(
    AudioContextAutoplayTest,
    AudioContextAutoplayTest,
    testing::Values(AutoplayPolicy::Type::kNoUserGestureRequired,
                    AutoplayPolicy::Type::kUserGestureRequired,
                    AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin,
                    AutoplayPolicy::Type::kDocumentUserActivationRequired));

}  // namespace blink
