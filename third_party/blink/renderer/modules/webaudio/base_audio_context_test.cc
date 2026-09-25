// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class BaseAudioContextForTest : public BaseAudioContext {
 public:
  explicit BaseAudioContextForTest(LocalDOMWindow& window)
      : BaseAudioContext(&window, ContextType::kRealtimeContext, 128) {
    UpdateStateIfNeeded();
  }

  bool IsPullingAudioGraph() const override { return false; }
  bool HasRealtimeConstraint() override { return true; }
  void NotifySourceNodeStart() override {}
  bool HandlePreRenderTasks(uint32_t,
                            const AudioIOPosition*,
                            const AudioCallbackMetric*,
                            base::TimeDelta,
                            const media::AudioGlitchInfo&) override {
    return false;
  }
  void HandlePostRenderTasks() override {}

  // Override to avoid crashing due to missing destination node.
  void ContextDestroyed() override {}

  bool HasPendingActivity() const override { return false; }

  void AddPendingPromiseResolverForTest(
      ScriptPromiseResolver<IDLUndefined>* resolver) {
    AddPendingPromiseResolver(resolver);
  }
};

class BaseAudioContextTest : public PageTestBase {};

// Tests that ensure that we properly garbage collect audio contexts while they
// have pending promises.
// For test motivation, see crbug.com/477005346.
TEST_F(BaseAudioContextTest, GCWhilePendingPromise) {
  V8TestingScope scope;

  WeakPersistent<BaseAudioContextForTest> weak_context;
  // Keep the resolver alive via a Persistent handle
  Persistent<ScriptPromiseResolver<IDLUndefined>> resolver;
  {
    BaseAudioContextForTest* audio_context =
        MakeGarbageCollected<BaseAudioContextForTest>(*GetFrame().DomWindow());
    weak_context = audio_context;

    resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
        scope.GetScriptState());
    resolver->Promise();

    audio_context->AddPendingPromiseResolverForTest(resolver);

    // audio_context goes out of scope and is now eligible for GC.
    // resolver is NOT eligible for GC because we have a Persistent handle.
  }
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(weak_context.Get(), nullptr);
  EXPECT_NE(resolver.Get(), nullptr);
}

TEST_F(BaseAudioContextTest, GCWhilePendingPromiseTeardownAtSameTime) {
  V8TestingScope scope;
  WeakPersistent<BaseAudioContextForTest> weak_context;
  WeakPersistent<ScriptPromiseResolver<IDLUndefined>> weak_resolver;
  {
    BaseAudioContextForTest* audio_context =
        MakeGarbageCollected<BaseAudioContextForTest>(*GetFrame().DomWindow());
    weak_context = audio_context;

    // Manually add a resolver to pending_promise_resolvers_.
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
        scope.GetScriptState());
    weak_resolver = resolver;
    resolver->Promise();
    audio_context->AddPendingPromiseResolverForTest(resolver);

    // audio_context goes out of scope.
  }
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(weak_context.Get(), nullptr);
  EXPECT_EQ(weak_resolver.Get(), nullptr);
}

}  // namespace blink
