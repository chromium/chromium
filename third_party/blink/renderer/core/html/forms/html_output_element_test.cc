// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_output_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(HTMLLinkElementSizesAttributeTest,
     setHTMLForProperty_updatesForAttribute) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* element = MakeGarbageCollected<HTMLOutputElement>(*document);
  EXPECT_EQ(g_null_atom, element->FastGetAttribute(html_names::kForAttr));
  element->htmlFor()->setValue(AtomicString("  strawberry "));
  EXPECT_EQ("  strawberry ", element->FastGetAttribute(html_names::kForAttr));
}

TEST(HTMLOutputElementTest, setForAttribute_updatesHTMLForPropertyValue) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* element = MakeGarbageCollected<HTMLOutputElement>(*document);
  DOMTokenList* for_tokens = element->htmlFor();
  EXPECT_EQ(g_null_atom, for_tokens->value());
  element->setAttribute(html_names::kForAttr, AtomicString("orange grape"));
  EXPECT_EQ("orange grape", for_tokens->value());
}

}  // namespace blink
