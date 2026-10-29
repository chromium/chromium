// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"

#include <limits>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_offline_audio_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiocontextrendersizecategory_unsignedlong.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "v8/include/v8.h"

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

TEST_F(OfflineAudioContextTest, CloseBeforeRendering) {
  ScopedOfflineAudioContextIncrementalRenderingForTest scoped_feature(true);
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setSampleRate(44100.0f);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(context);

  ScriptPromiseTester close_tester(
      scope.GetScriptState(),
      context->closeContext(scope.GetScriptState(), ASSERT_NO_EXCEPTION));
  close_tester.WaitUntilSettled();

  EXPECT_TRUE(close_tester.IsFulfilled());
  EXPECT_EQ(context->ContextState(), V8AudioContextState::Enum::kClosed);
}

TEST_F(OfflineAudioContextTest, CloseWhileRenderingIsSuspended) {
  ScopedOfflineAudioContextIncrementalRenderingForTest scoped_feature(true);
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setSampleRate(44100.0f);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(context);

  ScriptPromiseTester suspend_tester(
      scope.GetScriptState(),
      context->suspendContext(scope.GetScriptState(), 0.0,
                              ASSERT_NO_EXCEPTION));
  ScriptPromiseTester render_tester(
      scope.GetScriptState(),
      context->startOfflineRendering(scope.GetScriptState(), 128,
                                     ASSERT_NO_EXCEPTION));
  suspend_tester.WaitUntilSettled();
  ASSERT_TRUE(suspend_tester.IsFulfilled());
  ASSERT_EQ(context->ContextState(), V8AudioContextState::Enum::kSuspended);

  ScriptPromiseTester close_tester(
      scope.GetScriptState(),
      context->closeContext(scope.GetScriptState(), ASSERT_NO_EXCEPTION));
  close_tester.WaitUntilSettled();
  render_tester.WaitUntilSettled();

  EXPECT_TRUE(close_tester.IsFulfilled());
  EXPECT_TRUE(render_tester.IsRejected());
  EXPECT_EQ(context->ContextState(), V8AudioContextState::Enum::kClosed);
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

namespace {

// Returns `AudioBuffer.prototype` so a `then` accessor installed on it is only
// reached when a promise is resolved with an AudioBuffer.
v8::Local<v8::Object> GetAudioBufferPrototype(ScriptState* script_state) {
  AudioBuffer* buffer = AudioBuffer::Create(1, 1, 48000.0f);
  CHECK(buffer);
  return ToV8Traits<AudioBuffer>::ToV8(script_state, buffer)
      .As<v8::Object>()
      ->GetPrototype()
      .As<v8::Object>();
}

struct ReentrantStartRenderingState {
  Persistent<OfflineAudioContext> context;
  Persistent<ScriptState> script_state;
  v8::Global<v8::Promise> second_promise;
  v8::Global<v8::Promise> third_promise;
  Vector<V8AudioContextState::Enum> observed_states;
  bool triggered = false;
};

void ThenGetterStartRenderingTwice(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  auto* state = static_cast<ReentrantStartRenderingState*>(
      info.DataV2().As<v8::External>()->Value(
          v8::kExternalPointerTypeTagDefault));
  if (!state) {
    return;
  }
  state->observed_states.push_back(state->context->ContextState());
  if (state->triggered) {
    return;
  }
  state->triggered = true;

  DummyExceptionStateForTesting exception_state_1;
  ScriptPromise<AudioBuffer> second = state->context->startOfflineRendering(
      state->script_state, 128, exception_state_1);
  EXPECT_FALSE(exception_state_1.HadException());
  state->second_promise.Reset(isolate, second.V8Promise());

  DummyExceptionStateForTesting exception_state_2;
  ScriptPromise<AudioBuffer> third = state->context->startOfflineRendering(
      state->script_state, 128, exception_state_2);
  EXPECT_FALSE(exception_state_2.HadException());
  state->third_promise.Reset(isolate, third.V8Promise());
}

struct ReentrantCloseState {
  Persistent<OfflineAudioContext> context;
  Persistent<ScriptState> script_state;
  v8::Global<v8::Promise> second_promise;
  v8::Global<v8::Promise> close_promise;
  bool triggered = false;
};

void ThenGetterStartRenderingThenClose(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  auto* state =
      static_cast<ReentrantCloseState*>(info.DataV2().As<v8::External>()->Value(
          v8::kExternalPointerTypeTagDefault));
  if (!state) {
    return;
  }
  if (state->triggered) {
    return;
  }
  state->triggered = true;

  // Queue another chunk while the first one is still being finalized, then
  // close the context before that chunk gets a chance to start.
  DummyExceptionStateForTesting render_exception_state;
  ScriptPromise<AudioBuffer> second = state->context->startOfflineRendering(
      state->script_state, 128, render_exception_state);
  EXPECT_FALSE(render_exception_state.HadException());
  state->second_promise.Reset(isolate, second.V8Promise());

  DummyExceptionStateForTesting close_exception_state;
  ScriptPromise<IDLUndefined> close_promise =
      state->context->closeContext(state->script_state, close_exception_state);
  EXPECT_FALSE(close_exception_state.HadException());
  state->close_promise.Reset(isolate, close_promise.V8Promise());

  // A pending close() marks the context as cleared, so any further
  // startRendering() call must be rejected synchronously.
  DummyExceptionStateForTesting cleared_exception_state;
  state->context->startOfflineRendering(state->script_state, 128,
                                        cleared_exception_state);
  EXPECT_TRUE(cleared_exception_state.HadException());
  EXPECT_EQ(cleared_exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

}  // namespace

TEST_F(OfflineAudioContextTest, StartRenderingReentrancyViaThenableGetter) {
  ScopedOfflineAudioContextIncrementalRenderingForTest scoped_feature(true);
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(384);
  options->setSampleRate(48000.0f);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(context);

  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> v8_context = scope.GetContext();
  v8::Local<v8::Object> buffer_prototype =
      GetAudioBufferPrototype(scope.GetScriptState());

  ReentrantStartRenderingState state;
  state.context = context;
  state.script_state = scope.GetScriptState();

  v8::Local<v8::External> external =
      v8::External::New(isolate, &state, v8::kExternalPointerTypeTagDefault);
  v8::Local<v8::Function> getter =
      v8::Function::New(v8_context, &ThenGetterStartRenderingTwice, external)
          .ToLocalChecked();
  buffer_prototype->SetAccessorProperty(V8AtomicString(isolate, "then"),
                                        getter);
  // Remove the accessor on every exit path so the getter can never reach
  // `state` after it has gone out of scope.
  absl::Cleanup remove_then_getter = [&] {
    std::ignore =
        buffer_prototype->Delete(v8_context, V8AtomicString(isolate, "then"));
  };

  ScriptPromiseTester first_tester(
      scope.GetScriptState(),
      context->startOfflineRendering(scope.GetScriptState(), 128,
                                     ASSERT_NO_EXCEPTION));
  first_tester.WaitUntilSettled();
  ASSERT_TRUE(first_tester.IsFulfilled());
  ASSERT_TRUE(state.triggered);
  ASSERT_FALSE(state.second_promise.IsEmpty());
  ASSERT_FALSE(state.third_promise.IsEmpty());

  ScriptPromiseTester second_tester(
      scope.GetScriptState(), ScriptPromise<AudioBuffer>::FromV8Promise(
                                  isolate, state.second_promise.Get(isolate)));
  ScriptPromiseTester third_tester(
      scope.GetScriptState(), ScriptPromise<AudioBuffer>::FromV8Promise(
                                  isolate, state.third_promise.Get(isolate)));
  second_tester.WaitUntilSettled();
  third_tester.WaitUntilSettled();

  EXPECT_TRUE(second_tester.IsFulfilled());
  EXPECT_TRUE(third_tester.IsFulfilled());
  // The context must still be running while each chunk promise, including the
  // final one, is resolved; the close is queued after that resolution.
  ASSERT_EQ(state.observed_states.size(), 3u);
  EXPECT_EQ(state.observed_states[0], V8AudioContextState::Enum::kRunning);
  EXPECT_EQ(state.observed_states[1], V8AudioContextState::Enum::kRunning);
  EXPECT_EQ(state.observed_states[2], V8AudioContextState::Enum::kRunning);
  EXPECT_EQ(context->ContextState(), V8AudioContextState::Enum::kClosed);
}

TEST_F(OfflineAudioContextTest, CloseInThenableGetterDuringChunkResolution) {
  ScopedOfflineAudioContextIncrementalRenderingForTest scoped_feature(true);
  V8TestingScope scope;

  OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
  options->setNumberOfChannels(1);
  options->setLength(384);
  options->setSampleRate(48000.0f);
  OfflineAudioContext* context = OfflineAudioContext::Create(
      GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(context);

  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> v8_context = scope.GetContext();
  v8::Local<v8::Object> buffer_prototype =
      GetAudioBufferPrototype(scope.GetScriptState());

  ReentrantCloseState state;
  state.context = context;
  state.script_state = scope.GetScriptState();

  v8::Local<v8::External> external =
      v8::External::New(isolate, &state, v8::kExternalPointerTypeTagDefault);
  v8::Local<v8::Function> getter =
      v8::Function::New(v8_context, &ThenGetterStartRenderingThenClose,
                        external)
          .ToLocalChecked();
  buffer_prototype->SetAccessorProperty(V8AtomicString(isolate, "then"),
                                        getter);
  // Remove the accessor on every exit path so the getter can never reach
  // `state` after it has gone out of scope.
  absl::Cleanup remove_then_getter = [&] {
    std::ignore =
        buffer_prototype->Delete(v8_context, V8AtomicString(isolate, "then"));
  };

  ScriptPromiseTester first_tester(
      scope.GetScriptState(),
      context->startOfflineRendering(scope.GetScriptState(), 128,
                                     ASSERT_NO_EXCEPTION));
  first_tester.WaitUntilSettled();
  ASSERT_TRUE(first_tester.IsFulfilled());
  ASSERT_TRUE(state.triggered);
  ASSERT_FALSE(state.second_promise.IsEmpty());
  ASSERT_FALSE(state.close_promise.IsEmpty());

  ScriptPromiseTester second_tester(
      scope.GetScriptState(), ScriptPromise<AudioBuffer>::FromV8Promise(
                                  isolate, state.second_promise.Get(isolate)));
  ScriptPromiseTester close_tester(
      scope.GetScriptState(), ScriptPromise<IDLUndefined>::FromV8Promise(
                                  isolate, state.close_promise.Get(isolate)));
  close_tester.WaitUntilSettled();
  second_tester.WaitUntilSettled();

  EXPECT_TRUE(close_tester.IsFulfilled());
  EXPECT_TRUE(second_tester.IsRejected());
  EXPECT_EQ(context->ContextState(), V8AudioContextState::Enum::kClosed);
}

}  // namespace blink
