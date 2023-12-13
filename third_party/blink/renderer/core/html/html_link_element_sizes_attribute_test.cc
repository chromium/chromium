// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_link_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(HTMLLinkElementSizesAttributeTest,
     setSizesPropertyValue_updatesAttribute) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* link =
      MakeGarbageCollected<HTMLLinkElement>(*document, CreateElementFlags());
  DOMTokenList* sizes = link->sizes();
  EXPECT_EQ(g_null_atom, sizes->value());
  sizes->setValue(AtomicString("   a b  c "));
  EXPECT_EQ("   a b  c ", link->FastGetAttribute(html_names::kSizesAttr));
  EXPECT_EQ("   a b  c ", sizes->value());
}

TEST(HTMLLinkElementSizesAttributeTest,
     setSizesAttribute_updatesSizesPropertyValue) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* link =
      MakeGarbageCollected<HTMLLinkElement>(*document, CreateElementFlags());
  DOMTokenList* sizes = link->sizes();
  EXPECT_EQ(g_null_atom, sizes->value());
  link->setAttribute(html_names::kSizesAttr, AtomicString("y  x "));
  EXPECT_EQ("y  x ", sizes->value());
}

}  // namespace blink
