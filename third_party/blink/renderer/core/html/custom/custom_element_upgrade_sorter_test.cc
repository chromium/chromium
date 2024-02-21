// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_upgrade_sorter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CustomElementUpgradeSorterTest : public PageTestBase {
 protected:
  void SetUp() override { PageTestBase::SetUp(gfx::Size(1, 1)); }

  Element* CreateElementWithId(const char* local_name, const char* id) {
    NonThrowableExceptionState no_exceptions;
    Element* element = GetDocument().CreateElementForBinding(
        AtomicString(local_name), nullptr, no_exceptions);
    element->setAttribute(html_names::kIdAttr, AtomicString(id));
    return element;
  }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(&GetFrame());
  }
};

TEST_F(CustomElementUpgradeSorterTest, inOtherDocument_notInSet) {
  NonThrowableExceptionState no_exceptions;
  Element* element = GetDocument().CreateElementForBinding(
      AtomicString("a-a"), nullptr, no_exceptions);

  ScopedNullExecutionContext execution_context;
  auto* other_document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  other_document->AppendChild(element);
  EXPECT_EQ(other_document, element->ownerDocument())
      << "sanity: another document should have adopted an element on append";

  CustomElementUpgradeSorter sorter;
  sorter.Add(element);

  HeapVector<Member<Element>> elements;
  sorter.Sorted(&elements, &GetDocument());
  EXPECT_EQ(0u, elements.size())
      << "the adopted-away candidate should not have been included";
}

TEST_F(CustomElementUpgradeSorterTest, oneCandidate) {
  NonThrowableExceptionState no_exceptions;
  Element* element = GetDocument().CreateElementForBinding(
      AtomicString("a-a"), nullptr, no_exceptions);
  GetDocument().documentElement()->AppendChild(element);

  CustomElementUpgradeSorter sorter;
  sorter.Add(element);

  HeapVector<Member<Element>> elements;
  sorter.Sorted(&elements, &GetDocument());
  EXPECT_EQ(1u, elements.size())
      << "exactly one candidate should be in the result set";
  EXPECT_TRUE(elements.Contains(element))
      << "the candidate should be the element that was added";
}

TEST_F(CustomElementUpgradeSorterTest, candidatesInDocumentOrder) {
  Element* a = CreateElementWithId("a-a", "a");
  Element* b = CreateElementWithId("a-a", "b");
  Element* c = CreateElementWithId("a-a", "c");

  GetDocument().documentElement()->AppendChild(a);
  a->AppendChild(b);
  GetDocument().documentElement()->AppendChild(c);

  CustomElementUpgradeSorter sorter;
  sorter.Add(b);
  sorter.Add(a);
  sorter.Add(c);

  HeapVector<Member<Element>> elements;
  sorter.Sorted(&elements, &GetDocument());
  EXPECT_EQ(3u, elements.size());
  EXPECT_EQ(a, elements[0].Get());
  EXPECT_EQ(b, elements[1].Get());
  EXPECT_EQ(c, elements[2].Get());
}

TEST_F(CustomElementUpgradeSorterTest, sorter_ancestorInSet) {
  // A*
  // + B
  //   + C*
  Element* a = CreateElementWithId("a-a", "a");
  Element* b = CreateElementWithId("a-a", "b");
  Element* c = CreateElementWithId("a-a", "c");

  GetDocument().documentElement()->AppendChild(a);
  a->AppendChild(b);
  b->AppendChild(c);

  CustomElementUpgradeSorter sort;
  sort.Add(c);
  sort.Add(a);

  HeapVector<Member<Element>> elements;
  sort.Sorted(&elements, &GetDocument());
  EXPECT_EQ(2u, elements.size());
  EXPECT_EQ(a, elements[0].Get());
  EXPECT_EQ(c, elements[1].Get());
}

TEST_F(CustomElementUpgradeSorterTest, sorter_deepShallow) {
  // A
  // + B*
  // C*
  Element* a = CreateElementWithId("a-a", "a");
  Element* b = CreateElementWithId("a-a", "b");
  Element* c = CreateElementWithId("a-a", "c");

  GetDocument().documentElement()->AppendChild(a);
  a->AppendChild(b);
  GetDocument().documentElement()->AppendChild(c);

  CustomElementUpgradeSorter sort;
  sort.Add(b);
  sort.Add(c);

  HeapVector<Member<Element>> elements;
  sort.Sorted(&elements, &GetDocument());
  EXPECT_EQ(2u, elements.size());
  EXPECT_EQ(b, elements[0].Get());
  EXPECT_EQ(c, elements[1].Get());
}

TEST_F(CustomElementUpgradeSorterTest, sorter_shallowDeep) {
  // A*
  // B
  // + C*
  Element* a = CreateElementWithId("a-a", "a");
  Element* b = CreateElementWithId("a-a", "b");
  Element* c = CreateElementWithId("a-a", "c");

  GetDocument().documentElement()->AppendChild(a);
  GetDocument().documentElement()->AppendChild(b);
  b->AppendChild(c);

  CustomElementUpgradeSorter sort;
  sort.Add(a);
  sort.Add(c);

  HeapVector<Member<Element>> elements;
  sort.Sorted(&elements, &GetDocument());
  EXPECT_EQ(2u, elements.size());
  EXPECT_EQ(a, elements[0].Get());
  EXPECT_EQ(c, elements[1].Get());
}

TEST_F(CustomElementUpgradeSorterTest, sorter_shadow) {
  // A*
  // + {ShadowRoot}
  // | + B
  // |   + C*
  // + D*
  Element* a = CreateElementWithId("a-a", "a");
  Element* b = CreateElementWithId("a-a", "b");
  Element* c = CreateElementWithId("a-a", "c");
  Element* d = CreateElementWithId("a-a", "d");

  GetDocument().documentElement()->AppendChild(a);
  ShadowRoot* s = &a->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  a->AppendChild(d);

  s->AppendChild(b);
  b->AppendChild(c);

  CustomElementUpgradeSorter sort;
  sort.Add(a);
  sort.Add(c);
  sort.Add(d);

  HeapVector<Member<Element>> elements;
  sort.Sorted(&elements, &GetDocument());
  EXPECT_EQ(3u, elements.size());
  EXPECT_EQ(a, elements[0].Get());
  EXPECT_EQ(c, elements[1].Get());
  EXPECT_EQ(d, elements[2].Get());
}

}  // namespace blink
