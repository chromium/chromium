// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

namespace blink {

class V8ElementTest : public BindingTestSupportingGC {
 protected:
  void SetUp() override {
    // Precondition: test strings should not be in the AtomicStringTable yet.
    DCHECK(AtomicStringTable::Instance()
               .WeakFindForTesting("test-attribute")
               .IsNull());
    DCHECK(AtomicStringTable::Instance()
               .WeakFindForTesting("test-value")
               .IsNull());
  }

  void TearDown() override {
    PreciselyCollectGarbage();

    // Postcondition: test strings should have been released from the
    // AtomicStringTable
    DCHECK(AtomicStringTable::Instance()
               .WeakFindForTesting("test-attribute")
               .IsNull());
    DCHECK(AtomicStringTable::Instance()
               .WeakFindForTesting("test-value")
               .IsNull());
  }
};

v8::Local<v8::Value> Eval(const String& source, V8TestingScope& scope) {
  return ClassicScript::CreateUnspecifiedScript(source)
      ->RunScriptAndReturnValue(&scope.GetWindow())
      .GetSuccessValueOrEmpty();
}

TEST_F(V8ElementTest, SetAttributeOperationCallback) {
  V8TestingScope scope;

  Eval("document.body.setAttribute('test-attribute', 'test-value')", scope);
  EXPECT_FALSE(AtomicStringTable::Instance()
                   .WeakFindForTesting("test-attribute")
                   .IsNull());
  EXPECT_FALSE(
      AtomicStringTable::Instance().WeakFindForTesting("test-value").IsNull());

#if DCHECK_IS_ON()
  AtomicString test_attribute("test-attribute");
  EXPECT_EQ(test_attribute.Impl()->RefCountChangeCountForTesting(), 8u);
  AtomicString test_value("test-value");
  EXPECT_EQ(test_value.Impl()->RefCountChangeCountForTesting(), 6u);
#endif

  // Trigger a low memory notification. This will signal V8 to clear its
  // CompilationCache, which is needed because cached compiled code may be
  // holding references to externalized AtomicStrings.
  scope.GetIsolate()->LowMemoryNotification();
}

TEST_F(V8ElementTest, GetAttributeOperationCallback_NonExisting) {
  V8TestingScope scope;

  Eval("document.body.getAttribute('test-attribute')", scope);
  EXPECT_FALSE(AtomicStringTable::Instance()
                   .WeakFindForTesting("test-attribute")
                   .IsNull());
  EXPECT_TRUE(
      AtomicStringTable::Instance().WeakFindForTesting("test-value").IsNull());

#if DCHECK_IS_ON()
  AtomicString test_attribute("test-attribute");
  EXPECT_EQ(test_attribute.Impl()->RefCountChangeCountForTesting(), 5u);
#endif

  // Trigger a low memory notification. This will signal V8 to clear its
  // CompilationCache, which is needed because cached compiled code may be
  // holding references to externalized AtomicStrings.
  scope.GetIsolate()->LowMemoryNotification();
}

TEST_F(V8ElementTest, GetAttributeOperationCallback_Existing) {
  V8TestingScope scope;

  Eval("document.body.setAttribute('test-attribute', 'test-value')", scope);
  EXPECT_FALSE(AtomicStringTable::Instance()
                   .WeakFindForTesting("test-attribute")
                   .IsNull());
  EXPECT_FALSE(
      AtomicStringTable::Instance().WeakFindForTesting("test-value").IsNull());

#if DCHECK_IS_ON()
  AtomicString test_attribute("test-attribute");
  test_attribute.Impl()->ResetRefCountChangeCountForTesting();
  AtomicString test_value("test-value");
  test_value.Impl()->ResetRefCountChangeCountForTesting();
#endif

  Eval("document.body.getAttribute('test-attribute')", scope);

#if DCHECK_IS_ON()
  EXPECT_EQ(test_attribute.Impl()->RefCountChangeCountForTesting(), 4u);
  EXPECT_EQ(test_value.Impl()->RefCountChangeCountForTesting(), 2u);
#endif

  // Trigger a low memory notification. This will signal V8 to clear its
  // CompilationCache, which is needed because cached compiled code may be
  // holding references to externalized AtomicStrings.
  scope.GetIsolate()->LowMemoryNotification();
}

}  // namespace blink
