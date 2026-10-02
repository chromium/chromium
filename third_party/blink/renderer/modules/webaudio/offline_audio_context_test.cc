// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_offline_audio_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiocontextrendersizecategory_unsignedlong.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class OfflineAudioContextTest : public PageTestBase {};

TEST_F(OfflineAudioContextTest, RenderSizeHint) {
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(128);
  options->setSampleRate(44100.0);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(context->renderQuantumSize(), 128u);

  options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(128);
  options->setSampleRate(44100.0);
  options->setRenderSizeHint(
      MakeGarbageCollected<V8UnionAudioContextRenderSizeCategoryOrUnsignedLong>(
          0u));
  DummyExceptionStateForTesting exception_state_zero_hint;
  context = OfflineAudioContext::Create(GetFrame().DomWindow(), options,
                                        exception_state_zero_hint);
  EXPECT_TRUE(exception_state_zero_hint.HadException());
  EXPECT_EQ(exception_state_zero_hint.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotSupportedError);

  options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(128);
  options->setSampleRate(44100.0);
  options->setRenderSizeHint(
      MakeGarbageCollected<V8UnionAudioContextRenderSizeCategoryOrUnsignedLong>(
          264601u));
  DummyExceptionStateForTesting exception_state_too_large;
  context = OfflineAudioContext::Create(GetFrame().DomWindow(), options,
                                        exception_state_too_large);
  EXPECT_TRUE(exception_state_too_large.HadException());
  EXPECT_EQ(exception_state_too_large.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotSupportedError);

  options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(128);
  options->setSampleRate(44100.0);
  options->setRenderSizeHint(
      MakeGarbageCollected<V8UnionAudioContextRenderSizeCategoryOrUnsignedLong>(
          1u));
  context = OfflineAudioContext::Create(GetFrame().DomWindow(), options,
                                        ASSERT_NO_EXCEPTION);
  EXPECT_EQ(context->renderQuantumSize(), 1u);

  options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(128);
  options->setSampleRate(44100.0);
  options->setRenderSizeHint(
      MakeGarbageCollected<V8UnionAudioContextRenderSizeCategoryOrUnsignedLong>(
          16385u));
  context = OfflineAudioContext::Create(GetFrame().DomWindow(), options,
                                        ASSERT_NO_EXCEPTION);
  EXPECT_EQ(context->renderQuantumSize(), 16385u);

  options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(128);
  options->setSampleRate(44100.0);
  options->setRenderSizeHint(
      MakeGarbageCollected<V8UnionAudioContextRenderSizeCategoryOrUnsignedLong>(
          256u));
  context = OfflineAudioContext::Create(GetFrame().DomWindow(), options,
                                        ASSERT_NO_EXCEPTION);
  EXPECT_EQ(context->renderQuantumSize(), 256u);
}

TEST_F(OfflineAudioContextTest, EarlyCompletionZeroesDestinationBuffer) {
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(2);
  options->setLength(256);
  options->setSampleRate(44100.0f);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(context);

  // Suspend at frame 0.
  ScriptPromise<IDLUndefined> suspend_promise =
      context->suspendContext(scope.GetScriptState(), 0.0, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester suspend_tester(scope.GetScriptState(), suspend_promise);

  ScriptPromise<AudioBuffer> render_promise = context->startOfflineRendering(
      scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester render_tester(scope.GetScriptState(), render_promise);

  // Wait for suspend to trigger.
  suspend_tester.WaitUntilSettled();
  EXPECT_TRUE(suspend_tester.IsFulfilled());

  // Trigger allocation failure mid-render (while suspended).
  context->SetAllocationFailed();

  // Resume rendering.
  ScriptPromise<IDLUndefined> resume_promise =
      context->resumeContext(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester resume_tester(scope.GetScriptState(), resume_promise);
  resume_tester.WaitUntilSettled();
  EXPECT_TRUE(resume_tester.IsFulfilled());

  // Wait for rendering completion.
  render_tester.WaitUntilSettled();
  EXPECT_TRUE(render_tester.IsFulfilled());

  auto* buffer = ToScriptWrappable<AudioBuffer>(
      scope.GetIsolate(), render_tester.Value().V8Value().As<v8::Object>());
  ASSERT_TRUE(buffer);
  EXPECT_EQ(buffer->numberOfChannels(), 2u);
  EXPECT_EQ(buffer->length(), 256u);

  for (unsigned ch = 0; ch < buffer->numberOfChannels(); ++ch) {
    auto array = buffer->getChannelData(ch);
    ASSERT_TRUE(array);
    for (float sample : array->AsSpan()) {
      EXPECT_EQ(sample, 0.0f);
    }
  }
}

TEST_F(OfflineAudioContextTest, OfflineGCWhilePendingPromise) {
  V8TestingScope scope;
  WeakPersistent<OfflineAudioContext> weak_context;
  {
    OfflineAudioContext* audio_context = OfflineAudioContext::Create(
        GetFrame().DomWindow(), 1, 128, 44100, ASSERT_NO_EXCEPTION);
    weak_context = audio_context;

    // Create a pending promise in OfflineAudioContext (scheduled_suspends_).
    audio_context->suspendContext(scope.GetScriptState(), 0.001,
                                  ASSERT_NO_EXCEPTION);
  }
  // Trigger GC. This should call BaseAudioContext::Dispose, which for
  // OfflineAudioContext calls DetachPendingResolvers.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(weak_context.Get(), nullptr);
}

TEST_F(OfflineAudioContextTest, MultipleStartRenderingCalls) {
  ScopedOfflineAudioContextIncrementalRenderingForTest scoped_feature(true);
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(300);
  options->setSampleRate(44100.0f);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(context);

  ScriptPromiseTester first_tester(
      scope.GetScriptState(),
      context->startOfflineRendering(scope.GetScriptState(), 128,
                                     ASSERT_NO_EXCEPTION));
  ScriptPromiseTester second_tester(
      scope.GetScriptState(),
      context->startOfflineRendering(scope.GetScriptState(), 128,
                                     ASSERT_NO_EXCEPTION));
  ScriptPromiseTester third_tester(
      scope.GetScriptState(),
      context->startOfflineRendering(scope.GetScriptState(), 128,
                                     ASSERT_NO_EXCEPTION));

  first_tester.WaitUntilSettled();
  second_tester.WaitUntilSettled();
  third_tester.WaitUntilSettled();

  ASSERT_TRUE(first_tester.IsFulfilled());
  ASSERT_TRUE(second_tester.IsFulfilled());
  ASSERT_TRUE(third_tester.IsFulfilled());

  AudioBuffer* first_buffer = ToScriptWrappable<AudioBuffer>(
      scope.GetIsolate(), first_tester.Value().V8Value().As<v8::Object>());
  AudioBuffer* second_buffer = ToScriptWrappable<AudioBuffer>(
      scope.GetIsolate(), second_tester.Value().V8Value().As<v8::Object>());
  AudioBuffer* third_buffer = ToScriptWrappable<AudioBuffer>(
      scope.GetIsolate(), third_tester.Value().V8Value().As<v8::Object>());

  ASSERT_TRUE(first_buffer);
  ASSERT_TRUE(second_buffer);
  ASSERT_TRUE(third_buffer);
  EXPECT_EQ(first_buffer->length(), 128u);
  EXPECT_EQ(second_buffer->length(), 128u);
  EXPECT_EQ(third_buffer->length(), 44u);
}

TEST_F(OfflineAudioContextTest, IndefiniteChunkSizeRoundingOverflow) {
  ScopedOfflineAudioContextIncrementalRenderingForTest scoped_feature(true);
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setSampleRate(44100.0f);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(context);

  DummyExceptionStateForTesting exception_state;
  context->startOfflineRendering(scope.GetScriptState(),
                                 std::numeric_limits<uint32_t>::max(),
                                 exception_state);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotSupportedError);
  EXPECT_EQ(exception_state.Message(),
            "The requested chunk size is too large.");
}

TEST_F(OfflineAudioContextTest, MissingLengthWhenIncrementalRenderingDisabled) {
  ScopedOfflineAudioContextIncrementalRenderingForTest scoped_feature(false);
  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setSampleRate(44100.0);
  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, exception_state);

  EXPECT_EQ(context, nullptr);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<ESErrorType>(), ESErrorType::kTypeError);
  EXPECT_EQ(exception_state.Message(),
            "Failed to read the 'length' property from "
            "'OfflineAudioContextOptions': Required member is undefined.");
}

TEST_F(OfflineAudioContextTest,
       StartRenderingWhileRunningWhenIncrementalRenderingDisabled) {
  ScopedOfflineAudioContextIncrementalRenderingForTest scoped_feature(false);
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(128);
  options->setSampleRate(44100.0);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(context);

  ScriptPromiseTester render_tester(
      scope.GetScriptState(), context->startOfflineRendering(
                                  scope.GetScriptState(), ASSERT_NO_EXCEPTION));

  DummyExceptionStateForTesting exception_state;
  context->startOfflineRendering(scope.GetScriptState(), exception_state);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(exception_state.Message(),
            "cannot startRendering when an OfflineAudioContext is running");

  render_tester.WaitUntilSettled();
  EXPECT_TRUE(render_tester.IsFulfilled());
}

TEST_F(OfflineAudioContextTest,
       StartRenderingTwiceWhenIncrementalRenderingDisabled) {
  ScopedOfflineAudioContextIncrementalRenderingForTest scoped_feature(false);
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(128);
  options->setSampleRate(44100.0);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(context);

  ScriptPromiseTester suspend_tester(
      scope.GetScriptState(),
      context->suspendContext(scope.GetScriptState(), 0.0,
                              ASSERT_NO_EXCEPTION));
  ScriptPromiseTester render_tester(
      scope.GetScriptState(), context->startOfflineRendering(
                                  scope.GetScriptState(), ASSERT_NO_EXCEPTION));
  suspend_tester.WaitUntilSettled();
  ASSERT_TRUE(suspend_tester.IsFulfilled());

  DummyExceptionStateForTesting exception_state;
  context->startOfflineRendering(scope.GetScriptState(), exception_state);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(exception_state.Message(),
            "cannot call startRendering more than once");

  ScriptPromiseTester resume_tester(
      scope.GetScriptState(),
      context->resumeContext(scope.GetScriptState(), ASSERT_NO_EXCEPTION));
  resume_tester.WaitUntilSettled();
  EXPECT_TRUE(resume_tester.IsFulfilled());
  render_tester.WaitUntilSettled();
  EXPECT_TRUE(render_tester.IsFulfilled());
}

}  // namespace blink
