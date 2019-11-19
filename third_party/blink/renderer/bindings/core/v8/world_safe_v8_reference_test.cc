// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "v8/include/v8.h"

namespace blink {

class DummyPageHolder;
class KURL;

namespace {

class IsolateOnlyV8TestingScope {
  STACK_ALLOCATED();

 public:
  IsolateOnlyV8TestingScope(const KURL& url = KURL())
      : holder_(V8TestingScope::CreateDummyPageHolder(url)),
        handle_scope_(GetIsolate()) {}

  v8::Isolate* GetIsolate() const {
    return ToScriptStateForMainWorld(holder_->GetDocument().GetFrame())
        ->GetIsolate();
  }

 private:
  std::unique_ptr<DummyPageHolder> holder_;
  v8::HandleScope handle_scope_;
};

// http://crbug.com/1007504, http://crbug.com/1008425
TEST(WorldSafeV8ReferenceTest, CreatedWhenNotInContext) {
  WorldSafeV8Reference<v8::Value> v8_reference;
  v8::Local<v8::Value> value;
  {
    IsolateOnlyV8TestingScope scope1;
    v8::Isolate* isolate = scope1.GetIsolate();
    CHECK(isolate);
    CHECK(!isolate->InContext());

    value = v8::Null(isolate);
    v8_reference = WorldSafeV8Reference<v8::Value>(isolate, value);
    EXPECT_FALSE(v8_reference.IsEmpty());
  }
  V8TestingScope scope2;
  ScriptState* script_state = scope2.GetScriptState();
  EXPECT_EQ(v8_reference.Get(script_state), value);
}

}  // namespace

}  // namespace blink
