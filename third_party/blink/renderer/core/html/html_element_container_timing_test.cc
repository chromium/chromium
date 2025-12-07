// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
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

TEST_F(HTMLElementContainerTimingAttributesTest,
       SelfOrAncestorContainerTiming_AddContainerTiming) {
  SetBodyContent("<div id=a><div id=b></div></div><div id=c></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());

  element_a->setAttribute(html_names::kContainertimingAttr, g_empty_atom);

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
}

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_AddContainerTimingWithChildContainerTiming) {
  SetBodyContent(
      "<div id=a><div id=b containertiming></div></div><div id=c></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());

  element_a->setAttribute(html_names::kContainertimingAttr, g_empty_atom);

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
}

TEST_F(HTMLElementContainerTimingAttributesTest,
       SelfOrAncestorContainerTiming_RemoveContainerTiming) {
  SetBodyContent(
      "<div id=a containertiming><div id=b></div></div><div id=c></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());

  element_a->removeAttribute(html_names::kContainertimingAttr);

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
}

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_RemoveContainerTimingWithContainerTimingChild) {
  SetBodyContent(
      "<div id=a containertiming><div id=b containertiming></div></div><div "
      "id=c></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());

  element_a->removeAttribute(html_names::kContainertimingAttr);

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
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

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_MoveNonContainerTimingSubTreeToNonContainerTiming) {
  SetBodyContent(
      "<div id=a><div id=b><div id=c "
      "containertiming></div></div></div><div "
      "id=d></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());

  element_d->appendChild(element_b);

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());
}

TEST_F(HTMLElementContainerTimingAttributesTest,
       SelfOrAncestorContainerTiming_ContainerTimingIgnoreTree) {
  SetBodyContent(
      "<div id=a containertiming-ignore><div id=b></div></div><div "
      "id=c></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
}

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_ContainerTimingIgnoreInsideContainerTimingThenRemove) {
  SetBodyContent(
      "<div id=a containertiming><div id=b containertiming-ignore><div "
      "id=c></div></div></div><div id=d></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());

  element_b->removeAttribute(html_names::kContainertimingIgnoreAttr);

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());
}

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_ContainerTimingInsideContainerTimingIgnore) {
  SetBodyContent(
      "<div id=a containertiming-ignore><div id=b containertiming><div "
      "id=c></div></div></div><div id=d></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());
}

TEST_F(HTMLElementContainerTimingAttributesTest,
       SelfOrAncestorContainerTiming_ContainerTimingWithIgnore) {
  SetBodyContent(
      "<div id=a><div id=b containertiming containertiming-ignore><div "
      "id=c></div></div></div><div id=d></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());
}

TEST_F(
    HTMLElementContainerTimingAttributesTest,
    SelfOrAncestorContainerTiming_ContainerTimingWithIgnoreWithContainerTimingParent) {
  SetBodyContent(
      "<div id=a containertiming><div id=b containertiming "
      "containertiming-ignore><div "
      "id=c></div></div></div><div id=d></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());
}

TEST_F(HTMLElementContainerTimingAttributesTest,
       SelfOrAncestorContainerTiming_ContainerTimingWithIgnoreWithChildren) {
  SetBodyContent(
      "<div id=a containertiming containertiming-ignore><div id=b "
      "containertiming-ignore><div "
      "id=c></div></div></div><div id=d></div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));

  EXPECT_TRUE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());
}

TEST_F(HTMLElementContainerTimingAttributesTest,
       SelfOrAncestorContainerTiming_TreeWithContainerTimingAndIgnore) {
  SetBodyContent(
      "<div id=a>"
      "  <div id=b containertiming>"
      "    <div id=d>"
      "      <div id=h containertiming>"
      "        <div id=l></div>"
      "      </div>"
      "    </div>"
      "    <div id=e containertiming-ignore>"
      "      <div id=i></div>"
      "    </div>"
      "  </div>"
      "<div id=c containertiming-ignore>"
      "  <div id=f>"
      "    <div id=j containertiming containertiming-ignore>"
      "      <div id=m></div>"
      "    </div>"
      "  </div>"
      "  <div id=g containertiming-ignore>"
      "    <div id=k containertiming>"
      "      <div id=n></div>"
      "    </div>"
      "  </div>"
      "</div>");

  Element* element_a = GetDocument().getElementById(AtomicString("a"));
  Element* element_b = GetDocument().getElementById(AtomicString("b"));
  Element* element_c = GetDocument().getElementById(AtomicString("c"));
  Element* element_d = GetDocument().getElementById(AtomicString("d"));
  Element* element_e = GetDocument().getElementById(AtomicString("e"));
  Element* element_f = GetDocument().getElementById(AtomicString("f"));
  Element* element_g = GetDocument().getElementById(AtomicString("g"));
  Element* element_h = GetDocument().getElementById(AtomicString("h"));
  Element* element_i = GetDocument().getElementById(AtomicString("i"));
  Element* element_j = GetDocument().getElementById(AtomicString("j"));
  Element* element_k = GetDocument().getElementById(AtomicString("k"));
  Element* element_l = GetDocument().getElementById(AtomicString("l"));
  Element* element_m = GetDocument().getElementById(AtomicString("m"));
  Element* element_n = GetDocument().getElementById(AtomicString("n"));

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_d->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_e->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_f->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_g->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_h->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_i->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_j->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_k->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_l->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_m->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_n->SelfOrAncestorHasContainerTiming());

  element_h->AppendChild(element_f);

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_d->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_e->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_f->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_g->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_h->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_i->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_j->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_k->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_l->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_m->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_n->SelfOrAncestorHasContainerTiming());

  element_c->AppendChild(element_d);

  EXPECT_FALSE(element_a->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_b->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_c->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_d->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_e->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_f->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_g->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_h->SelfOrAncestorHasContainerTiming());
  EXPECT_FALSE(element_i->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_j->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_k->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_l->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_m->SelfOrAncestorHasContainerTiming());
  EXPECT_TRUE(element_n->SelfOrAncestorHasContainerTiming());
}

}  // namespace blink
