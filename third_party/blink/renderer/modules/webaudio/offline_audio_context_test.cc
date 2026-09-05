// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
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

}  // namespace blink
