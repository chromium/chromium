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
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
    observer.Observe(object);

    holder.Reset(GetIsolate(),
                 ToV8(object, scope.GetContext()->Global(), GetIsolate()));
  }

  RunV8MinorGC();
  PreciselyCollectGarbage();
  EXPECT_FALSE(observer.WasCollected());
  holder.Reset();
}

TEST_F(ScriptWrappableV8GCIntegrationTest,
       V8ReportsLiveObjectsDuringScavenger) {
  V8TestingScope scope;
  SetIsolate(scope.GetIsolate());

  GCObjectLivenessObserver<DeathAwareScriptWrappable> observer;
  {
    v8::HandleScope handle_scope(GetIsolate());
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
    observer.Observe(object);

    v8::Local<v8::Value> wrapper =
        ToV8(object, scope.GetContext()->Global(), GetIsolate());
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
  RunV8MinorGC();
  PreciselyCollectGarbage();

  EXPECT_FALSE(observer.WasCollected());
}

TEST_F(ScriptWrappableV8GCIntegrationTest,
       OilpanDoesntCollectObjectsReachableFromV8) {
  V8TestingScope scope;
  SetIsolate(scope.GetIsolate());

  v8::Persistent<v8::Value> holder;
  GCObjectLivenessObserver<DeathAwareScriptWrappable> observer;
  {
    v8::HandleScope handle_scope(GetIsolate());
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
    observer.Observe(object);

    // Creates new V8 wrapper and associates it with global scope
    holder.Reset(GetIsolate(),
                 ToV8(object, scope.GetContext()->Global(), GetIsolate()));
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
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
    observer.Observe(object);

    // Creates new V8 wrapper and associates it with global scope
    ToV8(object, scope.GetContext()->Global(), GetIsolate());
  }

  RunV8MinorGC();
  RunV8FullGC();
  PreciselyCollectGarbage();

  EXPECT_TRUE(observer.WasCollected());
}

}  // namespace blink
