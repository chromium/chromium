// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/testing/death_aware_script_wrappable.h"
#include "third_party/blink/renderer/core/testing/gc_object_liveness_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using ScriptWrappableV8GCIntegrationTest = BindingTestSupportingGC;

}  // namespace

// =============================================================================
// Tests that ScriptWrappable and its wrapper survive or are reclaimed in
// certain garbage collection scenarios.
// =============================================================================

TEST_F(ScriptWrappableV8GCIntegrationTest, V8ReportsLiveObjectsDuringFullGc) {
  V8TestingScope scope;
  SetIsolate(scope.GetIsolate());

  v8::Persistent<v8::Value> holder;
  GCObjectLivenessObserver<DeathAwareScriptWrappable> observer;
  {
    v8::HandleScope handle_scope(GetIsolate());
    auto* object = MakeGarbageCollected<DeathAwareScriptWrappable>();
    observer.Observe(object);

    holder.Reset(GetIsolate(), ToV8Traits<DeathAwareScriptWrappable>::ToV8(
                                   scope.GetScriptState(), object));
  }

  RunV8MinorGC();
  PreciselyCollectGarbage();
  EXPECT_FALSE(observer.WasCollected());
  holder.Reset();
}

TEST_F(ScriptWrappableV8GCIntegrationTest,
       OilpanDoesntCollectObjectsReachableFromV8) {
  V8TestingScope scope;
  SetIsolate(scope.GetIsolate());

  v8::Persistent<v8::Value> holder;
  GCObjectLivenessObserver<DeathAwareScriptWrappable> observer;
  {
    v8::HandleScope handle_scope(GetIsolate());
    auto* object = MakeGarbageCollected<DeathAwareScriptWrappable>();
    observer.Observe(object);

    // Creates new V8 wrapper and associates it with global scope
    holder.Reset(GetIsolate(), ToV8Traits<DeathAwareScriptWrappable>::ToV8(
                                   scope.GetScriptState(), object));
  }

  RunV8MinorGC();
  RunV8FullGC();
  PreciselyCollectGarbage();

  EXPECT_FALSE(observer.WasCollected());
  holder.Reset();
}

TEST_F(ScriptWrappableV8GCIntegrationTest,
       OilpanCollectObjectsNotReachableFromV8) {
  V8TestingScope scope;
  SetIsolate(scope.GetIsolate());

  GCObjectLivenessObserver<DeathAwareScriptWrappable> observer;
  {
    v8::HandleScope handle_scope(GetIsolate());
    auto* object = MakeGarbageCollected<DeathAwareScriptWrappable>();
    observer.Observe(object);

    // Creates new V8 wrapper and associates it with global scope
    ToV8Traits<DeathAwareScriptWrappable>::ToV8(scope.GetScriptState(), object)
        .IsEmpty();
  }

  RunV8MinorGC();
  RunV8FullGC();
  PreciselyCollectGarbage();

  EXPECT_TRUE(observer.WasCollected());
}

}  // namespace blink
