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

  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("a"))
                   ->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("b"))
                   ->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("c"))
                   ->SelfOrAncestorHasContainerTiming());
}

TEST_F(HTMLElementContainerTimingAttributesTest,
       SelfOrAncestorContainerTiming_ContainerTimingTree) {
  SetBodyContent(
      "<div id=a containertiming><div id=b></div></div><div id=c></div>");

  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("a"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("b"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("c"))
                   ->SelfOrAncestorHasContainerTiming());
}

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_MoveContainerTimingSubTreeToNonContainerTiming) {
  SetBodyContent(
      "<div id=a containertiming><div id=b><div id=c></div></div></div><div "
      "id=d></div>");

  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("a"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("b"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("c"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("d"))
                   ->SelfOrAncestorHasContainerTiming());

  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  element_d->appendChild(element_b);

  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("a"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("b"))
                   ->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("c"))
                   ->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("d"))
                   ->SelfOrAncestorHasContainerTiming());
}

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_MoveNonContainerTimingSubTreeToContainerTiming) {
  SetBodyContent(
      "<div id=a containertiming><div id=b><div id=c></div></div></div><div "
      "id=d></div>");

  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("a"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("b"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("c"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("d"))
                   ->SelfOrAncestorHasContainerTiming());

  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  element_c->appendChild(element_d);

  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("a"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("b"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("c"))
                  ->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(GetDocument()
                  .getElementById(AtomicString("d"))
                  ->SelfOrAncestorHasContainerTiming());
}
}  // namespace blink
