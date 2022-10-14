// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/boxed_v8_module.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

TEST(BoxedV8ModuleTest, equalAndHash) {
  V8TestingScope scope;
  const KURL js_url_a("https://example.com/a.js");
  const KURL js_url_b("https://example.com/b.js");

  Member<BoxedV8Module> module_empty = nullptr;
  Member<BoxedV8Module> module_deleted(kMemberDeletedValue);

  v8::Local<v8::Module> local_module_a = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const a = 'a';", js_url_a);
  Member<BoxedV8Module> module_a =
      MakeGarbageCollected<BoxedV8Module>(scope.GetIsolate(), local_module_a);
  v8::Local<v8::Module> local_module_b = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const b = 'b';", js_url_b);
  Member<BoxedV8Module> module_b =
      MakeGarbageCollected<BoxedV8Module>(scope.GetIsolate(), local_module_b);

  EXPECT_TRUE(BoxedV8ModuleHash::Equal(module_deleted, module_deleted));
  EXPECT_FALSE(BoxedV8ModuleHash::Equal(module_deleted, module_a));
  EXPECT_FALSE(BoxedV8ModuleHash::Equal(module_deleted, module_b));
  EXPECT_FALSE(BoxedV8ModuleHash::Equal(module_deleted, module_empty));

  EXPECT_TRUE(BoxedV8ModuleHash::Equal(module_empty, module_empty));
  EXPECT_FALSE(BoxedV8ModuleHash::Equal(module_empty, module_a));
  EXPECT_FALSE(BoxedV8ModuleHash::Equal(module_empty, module_b));

  EXPECT_TRUE(BoxedV8ModuleHash::Equal(module_a, module_a));
  EXPECT_FALSE(BoxedV8ModuleHash::Equal(module_a, module_b));

  EXPECT_NE(
      DefaultHash<blink::Member<blink::BoxedV8Module>>::GetHash(module_a),
      DefaultHash<blink::Member<blink::BoxedV8Module>>::GetHash(module_b));
}

}  // namespace

}  // namespace blink
