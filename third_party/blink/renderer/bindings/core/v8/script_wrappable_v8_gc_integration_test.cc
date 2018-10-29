// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/testing/death_aware_script_wrappable.h"
#include "third_party/blink/renderer/core/testing/gc_object_liveness_observer.h"
#include "v8/include/v8.h"

namespace blink {

namespace v8_gc_integration_test {

void PreciselyCollectGarbage() {
  ThreadState::Current()->CollectAllGarbage();
}

// The following directly calls testing GCs in V8 to avoid cluttering a globally
// visible interface with calls that have to be carefully staged.

void RunV8MinorGC(v8::Isolate* isolate) {
  CHECK(isolate);
  isolate->RequestGarbageCollectionForTesting(
      v8::Isolate::GarbageCollectionType::kMinorGarbageCollection);
}

void RunV8FullGCWithoutScanningOilpanStack(v8::Isolate* isolate) {
  CHECK(isolate);
  V8GCController::CollectAllGarbageForTesting(
      isolate, v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);
}

}  // namespace v8_gc_integration_test

// =============================================================================
// Tests that ScriptWrappable and its wrapper survive or are reclaimed in
// certain garbage collection scenarios.
// =============================================================================

TEST(ScriptWrappableV8GCIntegrationTest, V8ReportsLiveObjectsDuringFullGc) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  v8::Persistent<v8::Value> holder;
  GCObjectLivenessObserver<DeathAwareScriptWrappable> observer;
  {
    v8::HandleScope handle_scope(isolate);
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
    observer.Observe(object);

    holder.Reset(isolate, ToV8(object, scope.GetContext()->Global(), isolate));
  }

  v8_gc_integration_test::RunV8MinorGC(isolate);
  v8_gc_integration_test::PreciselyCollectGarbage();
  EXPECT_FALSE(observer.WasCollected());
  holder.Reset();
}

TEST(ScriptWrappableV8GCIntegrationTest, V8ReportsLiveObjectsDuringScavenger) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  GCObjectLivenessObserver<DeathAwareScriptWrappable> observer;
  {
    v8::HandleScope handle_scope(isolate);
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
    observer.Observe(object);

    v8::Local<v8::Value> wrapper =
        ToV8(object, scope.GetContext()->Global(), isolate);
    EXPECT_TRUE(wrapper->IsObject());
    v8::Local<v8::Object> wrapper_object =
        wrapper->ToObject(scope.GetContext()).ToLocalChecked();
    // V8 collects wrappers with unmodified maps (as they can be recreated
    // without losing any data if needed). We need to create some property on
    // wrapper so V8 will not see it as unmodified.
    EXPECT_TRUE(
        wrapper_object->CreateDataProperty(scope.GetContext(), 1, wrapper)
            .IsJust());
  }

  // Scavenger should not collect JavaScript wrappers that are modified, even if
  // they are otherwise unreachable.
  v8_gc_integration_test::RunV8MinorGC(isolate);
  v8_gc_integration_test::PreciselyCollectGarbage();

  EXPECT_FALSE(observer.WasCollected());
}

TEST(ScriptWrappableV8GCIntegrationTest,
     OilpanDoesntCollectObjectsReachableFromV8) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  v8::Persistent<v8::Value> holder;
  GCObjectLivenessObserver<DeathAwareScriptWrappable> observer;
  {
    v8::HandleScope handle_scope(isolate);
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
    observer.Observe(object);

    // Creates new V8 wrapper and associates it with global scope
    holder.Reset(isolate, ToV8(object, scope.GetContext()->Global(), isolate));
  }

  v8_gc_integration_test::RunV8MinorGC(isolate);
  v8_gc_integration_test::RunV8FullGCWithoutScanningOilpanStack(isolate);
  v8_gc_integration_test::PreciselyCollectGarbage();

  EXPECT_FALSE(observer.WasCollected());
  holder.Reset();
}

TEST(ScriptWrappableV8GCIntegrationTest,
     OilpanCollectObjectsNotReachableFromV8) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  GCObjectLivenessObserver<DeathAwareScriptWrappable> observer;
  {
    v8::HandleScope handle_scope(isolate);
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
    observer.Observe(object);

    // Creates new V8 wrapper and associates it with global scope
    ToV8(object, scope.GetContext()->Global(), isolate);
  }

  v8_gc_integration_test::RunV8MinorGC(isolate);
  v8_gc_integration_test::RunV8FullGCWithoutScanningOilpanStack(isolate);
  v8_gc_integration_test::PreciselyCollectGarbage();

  EXPECT_TRUE(observer.WasCollected());
}

}  // namespace blink
