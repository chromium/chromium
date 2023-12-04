// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/boxed_v8_module.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

TEST(BoxedV8ModuleTest, equalAndHash) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL js_url_a("https://example.com/a.js");
  const KURL js_url_b("https://example.com/b.js");

  v8::Local<v8::Module> local_module_a = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const a = 'a';", js_url_a);
  Member<BoxedV8Module> module_a =
      MakeGarbageCollected<BoxedV8Module>(scope.GetIsolate(), local_module_a);
  v8::Local<v8::Module> local_module_b = ModuleTestBase::CompileModule(
      scope.GetScriptState(), "export const b = 'b';", js_url_b);
  Member<BoxedV8Module> module_b =
      MakeGarbageCollected<BoxedV8Module>(scope.GetIsolate(), local_module_b);

  using Traits = HashTraits<blink::Member<blink::BoxedV8Module>>;
  static_assert(!Traits::kSafeToCompareToEmptyOrDeleted);

  EXPECT_TRUE(Traits::Equal(module_a, module_a));
  EXPECT_FALSE(Traits::Equal(module_a, module_b));

  EXPECT_NE(WTF::GetHash(module_a), WTF::GetHash(module_b));
}

}  // namespace

}  // namespace blink
