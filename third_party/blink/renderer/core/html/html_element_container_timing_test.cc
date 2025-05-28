// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLElementContainerTimingAttributesTest : public PageTestBase {};

TEST_F(HTMLElementContainerTimingAttributesTest,
       SelfOrAncestorContainerTiming_NoFlag) {
  SetBodyContent("<div id=a><div id=b></div></div><div id=c></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
}

TEST_F(HTMLElementContainerTimingAttributesTest,
       SelfOrAncestorContainerTiming_ContainerTimingTree) {
  SetBodyContent(
      "<div id=a containertiming><div id=b></div></div><div id=c></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
}

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_MoveContainerTimingSubTreeToNonContainerTiming) {
  SetBodyContent(
      "<div id=a containertiming><div id=b><div id=c></div></div></div><div "
      "id=d></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());

  element_d->appendChild(element_b);

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());
}

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_MoveNonContainerTimingSubTreeToContainerTiming) {
  SetBodyContent(
      "<div id=a containertiming><div id=b><div id=c></div></div></div><div "
      "id=d></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());

  element_c->appendChild(element_d);

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_d->SelfOrAncestorHasContainerTiming());
}

}  // namespace blink
