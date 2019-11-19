// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/slot_scoped_traversal.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class SlotScopedTraversalTest : public testing::Test {
 protected:
  Document& GetDocument() const;

  void SetupSampleHTML(const char* main_html,
                       const char* shadow_html,
                       unsigned);
  void AttachOpenShadowRoot(Element& shadow_host,
                            const char* shadow_inner_html);
  void TestExpectedSequence(const Vector<Element*>& list);

 private:
  void SetUp() override;

  Persistent<Document> document_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void SlotScopedTraversalTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  document_ = &dummy_page_holder_->GetDocument();
  DCHECK(document_);
}

Document& SlotScopedTraversalTest::GetDocument() const {
  return *document_;
}

void SlotScopedTraversalTest::SetupSampleHTML(const char* main_html,
                                              const char* shadow_html,
                                              unsigned index) {
  Element* body = GetDocument().body();
  body->SetInnerHTMLFromString(String::FromUTF8(main_html));
  if (shadow_html) {
    auto* shadow_host = To<Element>(NodeTraversal::ChildAt(*body, index));
    AttachOpenShadowRoot(*shadow_host, shadow_html);
  }
}

void SlotScopedTraversalTest::AttachOpenShadowRoot(
    Element& shadow_host,
    const char* shadow_inner_html) {
  ShadowRoot& shadow_root =
      shadow_host.AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.SetInnerHTMLFromString(String::FromUTF8(shadow_inner_html));
  GetDocument().body()->UpdateDistributionForFlatTreeTraversal();
}

TEST_F(SlotScopedTraversalTest, emptySlot) {
  const char* main_html = "<div id='host'></div>";
  const char* shadow_html = "<slot></slot>";
  SetupSampleHTML(main_html, shadow_html, 0);

  Element* host = GetDocument().QuerySelector("#host");
  ShadowRoot* shadow_root = host->OpenShadowRoot();
  auto* slot = To<HTMLSlotElement>(shadow_root->QuerySelector("slot"));

  EXPECT_EQ(nullptr, SlotScopedTraversal::FirstAssignedToSlot(*slot));
  EXPECT_EQ(nullptr, SlotScopedTraversal::LastAssignedToSlot(*slot));
}

void SlotScopedTraversalTest::TestExpectedSequence(
    const Vector<Element*>& list) {
  for (wtf_size_t i = 0; i < list.size(); ++i) {
    const Element* expected = i == list.size() - 1 ? nullptr : list[i + 1];
    EXPECT_EQ(expected, SlotScopedTraversal::Next(*list[i]));
  }

  for (wtf_size_t i = list.size(); i > 0; --i) {
    const Element* expected = i == 1 ? nullptr : list[i - 2];
    EXPECT_EQ(expected, SlotScopedTraversal::Previous(*list[i - 1]));
  }
}

TEST_F(SlotScopedTraversalTest, simpleSlot) {
  const char* main_html =
      "<div id='host'>"
      "<div id='inner1'></div>"
      "<div id='inner2'></div>"
      "</div>";

  const char* shadow_html = "<slot></slot>";

  SetupSampleHTML(main_html, shadow_html, 0);

  Element* host = GetDocument().QuerySelector("#host");
  Element* inner1 = GetDocument().QuerySelector("#inner1");
  Element* inner2 = GetDocument().QuerySelector("#inner2");
  ShadowRoot* shadow_root = host->OpenShadowRoot();
  auto* slot = To<HTMLSlotElement>(shadow_root->QuerySelector("slot"));

  EXPECT_EQ(inner1, SlotScopedTraversal::FirstAssignedToSlot(*slot));
  EXPECT_EQ(inner2, SlotScopedTraversal::LastAssignedToSlot(*slot));
  EXPECT_FALSE(SlotScopedTraversal::IsSlotScoped(*host));
  EXPECT_FALSE(SlotScopedTraversal::IsSlotScoped(*slot));
  EXPECT_TRUE(SlotScopedTraversal::IsSlotScoped(*inner1));
  EXPECT_TRUE(SlotScopedTraversal::IsSlotScoped(*inner2));
  EXPECT_EQ(inner1, SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(
                        *inner1));
  EXPECT_EQ(inner2, SlotScopedTraversal::NearestInclusiveAncestorAssignedToSlot(
                        *inner2));
  EXPECT_EQ(slot, SlotScopedTraversal::FindScopeOwnerSlot(*inner1));
  EXPECT_EQ(slot, SlotScopedTraversal::FindScopeOwnerSlot(*inner2));

  Vector<Element*> expected_sequence({inner1, inner2});
  TestExpectedSequence(expected_sequence);
}

TEST_F(SlotScopedTraversalTest, multipleSlots) {
  const char* main_html =
      "<div id='host'>"
      "<div id='inner0' slot='slot0'></div>"
      "<div id='inner1' slot='slot1'></div>"
      "<div id='inner2'></div>"
      "<div id='inner3' slot='slot1'></div>"
      "<div id='inner4' slot='slot0'></div>"
      "<div id='inner5'></div>"
      "</div>";

  const char* shadow_html =
      "<slot id='unnamedslot'></slot>"
      "<slot id='slot0' name='slot0'></slot>"
      "<slot id='slot1' name='slot1'></slot>";

  SetupSampleHTML(main_html, shadow_html, 0);

  Element* host = GetDocument().QuerySelector("#host");
  Element* inner[6];
  inner[0] = GetDocument().QuerySelector("#inner0");
  inner[1] = GetDocument().QuerySelector("#inner1");
  inner[2] = GetDocument().QuerySelector("#inner2");
  inner[3] = GetDocument().QuerySelector("#inner3");
  inner[4] = GetDocument().QuerySelector("#inner4");
  inner[5] = GetDocument().QuerySelector("#inner5");

  ShadowRoot* shadow_root = host->OpenShadowRoot();
  Element* slot_element[3];
  slot_element[0] = shadow_root->QuerySelector("#slot0");
  slot_element[1] = shadow_root->QuerySelector("#slot1");
  slot_element[2] = shadow_root->QuerySelector("#unnamedslot");

  HTMLSlotElement* slot[3];
  slot[0] = To<HTMLSlotElement>(slot_element[0]);
  slot[1] = To<HTMLSlotElement>(slot_element[1]);
  slot[2] = To<HTMLSlotElement>(slot_element[2]);

  {
    // <slot id='slot0'> : Expected assigned nodes: inner0, inner4
    EXPECT_EQ(inner[0], SlotScopedTraversal::FirstAssignedToSlot(*slot[0]));
    EXPECT_EQ(inner[4], SlotScopedTraversal::LastAssignedToSlot(*slot[0]));
    Vector<Element*> expected_sequence({inner[0], inner[4]});
    TestExpectedSequence(expected_sequence);
  }

  {
    // <slot name='slot1'> : Expected assigned nodes: inner1, inner3
    EXPECT_EQ(inner[1], SlotScopedTraversal::FirstAssignedToSlot(*slot[1]));
    EXPECT_EQ(inner[3], SlotScopedTraversal::LastAssignedToSlot(*slot[1]));
    Vector<Element*> expected_sequence({inner[1], inner[3]});
    TestExpectedSequence(expected_sequence);
  }

  {
    // <slot id='unnamedslot'> : Expected assigned nodes: inner2, inner5
    EXPECT_EQ(inner[2], SlotScopedTraversal::FirstAssignedToSlot(*slot[2]));
    EXPECT_EQ(inner[5], SlotScopedTraversal::LastAssignedToSlot(*slot[2]));
    Vector<Element*> expected_sequence({inner[2], inner[5]});
    TestExpectedSequence(expected_sequence);
  }
}

TEST_F(SlotScopedTraversalTest, shadowHostAtTopLevel) {
  // This covers a shadow host is directly assigned to a slot.
  //
  // We build the following tree:
  //         host
  //           |
  //       ShadowRoot
  //           |
  //     ____<slot>____
  //     |      |      |
  //   inner0 inner1 inner2
  //     |      |      |
  //   child0 child1 child2
  //
  // And iterate on inner0, inner1, and inner2 to attach shadow on each of
  // them, and check if elements that are slotted to another slot are skipped
  // in traversal.

  const char* main_html =
      "<div id='host'>"
      "<div id='inner0'><div id='child0'></div></div>"
      "<div id='inner1'><div id='child1'></div></div>"
      "<div id='inner2'><div id='child2'></div></div>"
      "</div>";

  const char* shadow_html = "<slot></slot>";

  for (int i = 0; i < 3; ++i) {
    SetupSampleHTML(main_html, shadow_html, 0);

    Element* host = GetDocument().QuerySelector("#host");
    Element* inner[3];
    inner[0] = GetDocument().QuerySelector("#inner0");
    inner[1] = GetDocument().QuerySelector("#inner1");
    inner[2] = GetDocument().QuerySelector("#inner2");
    Element* child[3];
    child[0] = GetDocument().QuerySelector("#child0");
    child[1] = GetDocument().QuerySelector("#child1");
    child[2] = GetDocument().QuerySelector("#child2");

    AttachOpenShadowRoot(*inner[i], shadow_html);

    ShadowRoot* shadow_root = host->OpenShadowRoot();
    auto* slot = To<HTMLSlotElement>(shadow_root->QuerySelector("slot"));

    switch (i) {
      case 0: {
        // inner0 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::FirstAssignedToSlot(*slot));
        EXPECT_EQ(child[2], SlotScopedTraversal::LastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::Next(*child[0]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::Previous(*child[0]));

        Vector<Element*> expected_sequence(
            {inner[0], inner[1], child[1], inner[2], child[2]});
        TestExpectedSequence(expected_sequence);
      } break;

      case 1: {
        // inner1 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::FirstAssignedToSlot(*slot));
        EXPECT_EQ(child[2], SlotScopedTraversal::LastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::Next(*child[1]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::Previous(*child[1]));

        Vector<Element*> expected_sequence(
            {inner[0], child[0], inner[1], inner[2], child[2]});
        TestExpectedSequence(expected_sequence);
      } break;

      case 2: {
        // inner2 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::FirstAssignedToSlot(*slot));
        EXPECT_EQ(inner[2], SlotScopedTraversal::LastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::Next(*child[2]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::Previous(*child[2]));

        Vector<Element*> expected_sequence(
            {inner[0], child[0], inner[1], child[1], inner[2]});
        TestExpectedSequence(expected_sequence);
      } break;
    }
  }
}

TEST_F(SlotScopedTraversalTest, shadowHostAtSecondLevel) {
  // This covers cases where a shadow host exists in a child of a slotted
  // element.
  //
  // We build the following tree:
  //            host
  //              |
  //          ShadowRoot
  //              |
  //        ____<slot>____
  //        |             |
  //     _inner0_      _inner1_
  //     |      |      |      |
  //   child0 child1 child2 child3
  //     |      |      |      |
  //    span0  span1  span2  span3
  //
  // And iterate on child0, child1, child2, and child3 to attach shadow on each
  // of them, and check if elements that are slotted to another slot are skipped
  // in traversal.

  const char* main_html =
      "<div id='host'>"
      "<div id='inner0'>"
      "<div id='child0'><span id='span0'></span></div>"
      "<div id='child1'><span id='span1'></span></div>"
      "</div>"
      "<div id='inner1'>"
      "<div id='child2'><span id='span2'></span></div>"
      "<div id='child3'><span id='span3'></span></div>"
      "</div>"
      "</div>";

  const char* shadow_html = "<slot></slot>";

  for (int i = 0; i < 4; ++i) {
    SetupSampleHTML(main_html, shadow_html, 0);

    Element* host = GetDocument().QuerySelector("#host");
    Element* inner[2];
    inner[0] = GetDocument().QuerySelector("#inner0");
    inner[1] = GetDocument().QuerySelector("#inner1");
    Element* child[4];
    child[0] = GetDocument().QuerySelector("#child0");
    child[1] = GetDocument().QuerySelector("#child1");
    child[2] = GetDocument().QuerySelector("#child2");
    child[3] = GetDocument().QuerySelector("#child3");
    Element* span[4];
    span[0] = GetDocument().QuerySelector("#span0");
    span[1] = GetDocument().QuerySelector("#span1");
    span[2] = GetDocument().QuerySelector("#span2");
    span[3] = GetDocument().QuerySelector("#span3");

    AttachOpenShadowRoot(*child[i], shadow_html);

    for (int j = 0; j < 4; ++j) {
      DCHECK(child[i]);
      DCHECK(span[i]);
    }

    ShadowRoot* shadow_root = host->OpenShadowRoot();
    auto* slot = To<HTMLSlotElement>(shadow_root->QuerySelector("slot"));

    switch (i) {
      case 0: {
        // child0 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::FirstAssignedToSlot(*slot));
        EXPECT_EQ(span[3], SlotScopedTraversal::LastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::Next(*span[0]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::Previous(*span[0]));

        const Vector<Element*> expected_sequence({inner[0], child[0], child[1],
                                                  span[1], inner[1], child[2],
                                                  span[2], child[3], span[3]});
        TestExpectedSequence(expected_sequence);
      } break;

      case 1: {
        // child1 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::FirstAssignedToSlot(*slot));
        EXPECT_EQ(span[3], SlotScopedTraversal::LastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::Next(*span[1]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::Previous(*span[1]));

        const Vector<Element*> expected_sequence({inner[0], child[0], span[0],
                                                  child[1], inner[1], child[2],
                                                  span[2], child[3], span[3]});
        TestExpectedSequence(expected_sequence);
      } break;

      case 2: {
        // child2 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::FirstAssignedToSlot(*slot));
        EXPECT_EQ(span[3], SlotScopedTraversal::LastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::Next(*span[2]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::Previous(*span[2]));

        const Vector<Element*> expected_sequence({inner[0], child[0], span[0],
                                                  child[1], span[1], inner[1],
                                                  child[2], child[3], span[3]});
        TestExpectedSequence(expected_sequence);
      } break;

      case 3: {
        // child3 is a shadow host.
        EXPECT_EQ(inner[0], SlotScopedTraversal::FirstAssignedToSlot(*slot));
        EXPECT_EQ(child[3], SlotScopedTraversal::LastAssignedToSlot(*slot));

        EXPECT_EQ(nullptr, SlotScopedTraversal::Next(*span[3]));
        EXPECT_EQ(nullptr, SlotScopedTraversal::Previous(*span[3]));

        const Vector<Element*> expected_sequence({inner[0], child[0], span[0],
                                                  child[1], span[1], inner[1],
                                                  child[2], span[2], child[3]});
        TestExpectedSequence(expected_sequence);
      } break;
    }
  }
}

}  // namespace blink
