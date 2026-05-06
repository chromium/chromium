// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_offline_audio_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiocontextrendersizecategory_unsignedlong.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_buffer.h"

namespace blink {

class OfflineAudioContextTest : public PageTestBase {};

TEST_F(OfflineAudioContextTest, RenderSizeHint) {
  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "WebAudioConfigurableRenderQuantum", true);
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

  blink::WebRuntimeFeatures::EnableFeatureFromString(
      "WebAudioConfigurableRenderQuantum", false);
}

TEST_F(OfflineAudioContextTest, OfflineRenderingThreadSafetyAndNoLeak) {
  V8TestingScope scope;

  // Synchronously collect any uncollected garbage from previous tests to
  // establish a clean baseline. Since InstanceCounters is a process-wide
  // global counter, this GC run isolates our leak check and guarantees
  // that the starting count is stable. This follows Chrome's official
  // automated leak detector pattern (blink_leak_detector.cc), which
  // queries kAudioHandlerCounter to find leaks.
  WebHeap::CollectAllGarbageForTesting();

  int initial_handler_count =
      InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter);

  {
    OfflineAudioContextOptions* options = OfflineAudioContextOptions::Create();
    options->setNumberOfChannels(1);
    // Render 10 render quanta (1280 frames) to fully exercise the background
    // thread rendering loop.
    options->setLength(1280);
    options->setSampleRate(44100.0);

    OfflineAudioContext* context = OfflineAudioContext::Create(
        GetFrame().DomWindow(), options, ASSERT_NO_EXCEPTION);

    ScriptPromise<AudioBuffer> promise = context->startOfflineRendering(
        scope.GetScriptState(), ASSERT_NO_EXCEPTION);

    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();

    EXPECT_TRUE(tester.IsFulfilled());
  }

  // Force complete garbage collection of both Blink Oilpan and V8 heaps
  // immediately. When it returns, all unreferenced contexts and nodes are
  // guaranteed to have been destroyed.
  WebHeap::CollectAllGarbageForTesting();

  // Verify that the count of active AudioHandlers returns to our baseline.
  int final_handler_count =
      InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter);
  EXPECT_EQ(final_handler_count, initial_handler_count);
}

}  // namespace blink
