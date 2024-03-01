// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/shadow_including_tree_order_traversal.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
namespace {

using ShadowIncludingTreeOrderTraversalTest = PageTestBase;
using ::testing::ElementsAre;

// Traverses `traversal_range` and collects the element ids of the `Element`s it
// discovers. If a node is a shadow root, it collects the string "shadow"
// instead.
Vector<String> GatherElementIdsFromTraversalRange(auto traversal_range) {
  Vector<String> ids;
  for (Node& node : traversal_range) {
    if (auto* el = DynamicTo<Element>(node)) {
      ids.push_back(el->GetIdAttribute());
    } else if (node.IsShadowRoot()) {
      ids.push_back("shadow");
    }
  }
  return ids;
}

void RemoveWhiteSpaceOnlyTextNodes(ContainerNode& container) {
  HeapVector<Member<Text>> to_remove;
  for (Node& descendant : NodeTraversal::DescendantsOf(container)) {
    if (auto* text = DynamicTo<Text>(&descendant);
        text && text->ContainsOnlyWhitespaceOrEmpty()) {
      to_remove.push_back(text);
    }
  }

  for (Text* text : to_remove)
    text->remove();
}

TEST_F(ShadowIncludingTreeOrderTraversalTest, Next) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id="c0">
      <div id="c00">
        <template shadowrootmode="open"></template>
      </div>
      <div id="c01">
        <template shadowrootmode="open">
          <div id="s0"></div>
          <div id="s1">
            <div id="s10"></div>
          </div>
        </template>
      </div>
      <div id="c02">
        <div id="c020"></div>
        <div id="c021" slot="t01"></div>
        <template shadowrootmode="open">
          <div id="t0">
            <slot id="t00"></slot>
            <slot id="t01"></slot>
          </div>
        </template>
      </div>
    </div>
    <div id="c1"></div>
  )HTML");
  RemoveWhiteSpaceOnlyTextNodes(*GetDocument().body());
  auto* c0 = GetElementById("c0");
  auto* c1 = GetElementById("c1");
  auto* c00 = GetElementById("c00");
  auto* c01 = GetElementById("c01");
  auto* c02 = GetElementById("c02");
  auto* c020 = GetElementById("c020");
  auto* c021 = GetElementById("c021");

  ShadowRoot* shadow_root_0 = c00->GetShadowRoot();
  ASSERT_TRUE(shadow_root_0);

  ShadowRoot* shadow_root_1 = c01->GetShadowRoot();
  ASSERT_TRUE(shadow_root_1);
  RemoveWhiteSpaceOnlyTextNodes(*shadow_root_1);
  auto* s0 = shadow_root_1->getElementById(AtomicString("s0"));
  auto* s1 = shadow_root_1->getElementById(AtomicString("s1"));
  auto* s10 = shadow_root_1->getElementById(AtomicString("s10"));

  ShadowRoot* shadow_root_2 = c02->GetShadowRoot();
  ASSERT_TRUE(shadow_root_2);
  RemoveWhiteSpaceOnlyTextNodes(*shadow_root_2);
  auto* t0 = shadow_root_2->getElementById(AtomicString("t0"));
  auto* t00 = shadow_root_2->getElementById(AtomicString("t00"));
  auto* t01 = shadow_root_2->getElementById(AtomicString("t01"));

  // Test iteration order using Next.
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*GetDocument().body(),
                                                    &GetDocument()),
            c0);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*c0, &GetDocument()), c00);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*c00, &GetDocument()),
            shadow_root_0);
  EXPECT_EQ(
      ShadowIncludingTreeOrderTraversal::Next(*shadow_root_0, &GetDocument()),
      c01);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*c01, &GetDocument()),
            shadow_root_1);
  EXPECT_EQ(
      ShadowIncludingTreeOrderTraversal::Next(*shadow_root_1, &GetDocument()),
      s0);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*s0, &GetDocument()), s1);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*s1, &GetDocument()), s10);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*s10, &GetDocument()), c02);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*c02, &GetDocument()),
            shadow_root_2);
  EXPECT_EQ(
      ShadowIncludingTreeOrderTraversal::Next(*shadow_root_2, &GetDocument()),
      t0);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*t0, &GetDocument()), t00);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*t00, &GetDocument()), t01);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*t01, &GetDocument()),
            c020);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*c020, &GetDocument()),
            c021);
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*c021, &GetDocument()), c1);

  // c1 is not in c0's tree, so this returns nullptr.
  EXPECT_EQ(ShadowIncludingTreeOrderTraversal::Next(*c021, c0), nullptr);
}

TEST_F(ShadowIncludingTreeOrderTraversalTest, DescendantsOf) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id="a0">
      <div id="a00"></div>
      <div id="a01"></div>
    </div>
    <div id="a1">
      <template shadowrootmode="open" id="sr1">
        <div id="b0">
          <div id="b00"></div>
        </div>
      </template>
      <div id="a10"></div>
    </div>
    <div id="a2"></div>
  )HTML");
  GetDocument().body()->SetIdAttribute(AtomicString("body"));

  EXPECT_THAT(GatherElementIdsFromTraversalRange(
                  ShadowIncludingTreeOrderTraversal::DescendantsOf(
                      *GetDocument().body())),
              ElementsAre("a0", "a00", "a01", "a1", "shadow", "b0", "b00",
                          "a10", "a2"));
  EXPECT_THAT(GatherElementIdsFromTraversalRange(
                  ShadowIncludingTreeOrderTraversal::InclusiveDescendantsOf(
                      *GetDocument().body())),
              ElementsAre("body", "a0", "a00", "a01", "a1", "shadow", "b0",
                          "b00", "a10", "a2"));
}

TEST_F(ShadowIncludingTreeOrderTraversalTest, ChildrenOf) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id="a0">
      <div id="a00"></div>
      <div id="a01"></div>
    </div>
    <div id="a1">
      <template shadowrootmode="open">
        <div id="b0">
          <div id="b00"></div>
        </div>
      </template>
      <div id="a10"></div>
    </div>
    <div id="a2">
      <template shadowrootmode="open">
        <slot></slot>
      </template>
      <div id="a20"></div>
    </div>
  )HTML");

  EXPECT_THAT(
      GatherElementIdsFromTraversalRange(
          ShadowIncludingTreeOrderTraversal::ChildrenOf(*GetDocument().body())),
      ElementsAre("a0", "a1", "a2"));
  EXPECT_THAT(
      GatherElementIdsFromTraversalRange(
          ShadowIncludingTreeOrderTraversal::ChildrenOf(*GetElementById("a1"))),
      ElementsAre("shadow", "a10"));
  EXPECT_THAT(
      GatherElementIdsFromTraversalRange(
          ShadowIncludingTreeOrderTraversal::ChildrenOf(*GetElementById("a2"))),
      ElementsAre("shadow", "a20"));
}

}  // namespace
}  // namespace blink
