// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_slot_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {
constexpr int kTableSize = 16;
using Seq = Vector<char>;
using Backtrack = std::pair<wtf_size_t, wtf_size_t>;
}

class HTMLSlotElementTest : public testing::Test {
 protected:
  HTMLSlotElementTest()
      : lcs_table_(kTableSize), backtrack_table_(kTableSize) {}
  Seq LongestCommonSubsequence(const Seq& seq1, const Seq& seq2);
  Vector<HTMLSlotElement::LCSArray<size_t, kTableSize>, kTableSize> lcs_table_;
  Vector<HTMLSlotElement::LCSArray<Backtrack, kTableSize>, kTableSize>
      backtrack_table_;
  test::TaskEnvironment task_environment_;
};

Vector<char> HTMLSlotElementTest::LongestCommonSubsequence(const Seq& seq1,
                                                           const Seq& seq2) {
  HTMLSlotElement::FillLongestCommonSubsequenceDynamicProgrammingTable(
      seq1, seq2, lcs_table_, backtrack_table_);
  Seq lcs;
  wtf_size_t r = seq1.size();
  wtf_size_t c = seq2.size();
  while (r > 0 && c > 0) {
    Backtrack backtrack = backtrack_table_[r][c];
    if (backtrack == std::make_pair(r - 1, c - 1)) {
      DCHECK_EQ(seq1[r - 1], seq2[c - 1]);
      lcs.push_back(seq1[r - 1]);
    }
    std::tie(r, c) = backtrack;
  }
  std::ranges::reverse(lcs);
  EXPECT_EQ(lcs_table_[seq1.size()][seq2.size()], lcs.size());
  return lcs;
}

TEST_F(HTMLSlotElementTest, LongestCommonSubsequence) {
  const Seq kEmpty;
  {
    Seq seq1{};
    Seq seq2{};
    EXPECT_EQ(kEmpty, LongestCommonSubsequence(seq1, seq2));
  }
  {
    Seq seq1{'a'};
    Seq seq2{};
    EXPECT_EQ(kEmpty, LongestCommonSubsequence(seq1, seq2));
  }
  {
    Seq seq1{};
    Seq seq2{'a'};
    EXPECT_EQ(kEmpty, LongestCommonSubsequence(seq1, seq2));
  }
  {
    Seq seq1{'a'};
    Seq seq2{'a'};
    EXPECT_EQ(Seq{'a'}, LongestCommonSubsequence(seq1, seq2));
  }
  {
    Seq seq1{'a', 'b'};
    Seq seq2{'a'};
    EXPECT_EQ(Seq{'a'}, LongestCommonSubsequence(seq1, seq2));
  }
  {
    Seq seq1{'a', 'b'};
    Seq seq2{'b', 'a'};
    EXPECT_TRUE(LongestCommonSubsequence(seq1, seq2) == Seq{'a'} ||
                LongestCommonSubsequence(seq1, seq2) == Seq{'b'});
  }
  {
    Seq seq1{'a', 'b', 'c', 'd'};
    Seq seq2{};
    EXPECT_EQ(kEmpty, LongestCommonSubsequence(seq1, seq2));
  }
  {
    Seq seq1{'a', 'b', 'c', 'd'};
    Seq seq2{'1', 'a', 'b', 'd'};
    Seq lcs{'a', 'b', 'd'};
    EXPECT_EQ(lcs, LongestCommonSubsequence(seq1, seq2));
  }
  {
    Seq seq1{'a', 'b', 'c', 'd'};
    Seq seq2{'b', 'a', 'c'};
    Seq lcs1{'a', 'c'};
    Seq lcs2{'b', 'c'};
    EXPECT_TRUE(LongestCommonSubsequence(seq1, seq2) == lcs1 ||
                LongestCommonSubsequence(seq1, seq2) == lcs2);
  }
  {
    Seq seq1{'a', 'b', 'c', 'd'};
    Seq seq2{'1', 'b', '2', 'd', '1'};
    Seq lcs{'b', 'd'};
    EXPECT_EQ(lcs, LongestCommonSubsequence(seq1, seq2));
  }
  {
    Seq seq1{'a', 'b', 'c', 'd'};
    Seq seq2{'a', 'd'};
    Seq lcs{'a', 'd'};
    EXPECT_EQ(lcs, LongestCommonSubsequence(seq1, seq2));
  }
  {
    Seq seq1{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    Seq seq2{'g', 'a', 'b', '1', 'd', '2', '3', 'h', '4'};
    Seq lcs{'a', 'b', 'd', 'h'};
    EXPECT_EQ(lcs, LongestCommonSubsequence(seq1, seq2));
  }
}

TEST_F(HTMLSlotElementTest, TableSizeLimit) {
  Seq seq1;
  // If we use kTableSize here, it should hit DCHECK().
  std::fill_n(std::back_inserter(seq1), kTableSize - 1, 'a');
  Seq seq2;
  std::fill_n(std::back_inserter(seq2), kTableSize - 1, 'a');
  Seq lcs;
  std::fill_n(std::back_inserter(lcs), kTableSize - 1, 'a');
  EXPECT_EQ(lcs, LongestCommonSubsequence(seq1, seq2));
}

class HTMLSlotElementInDocumentTest : public testing::Test {
 protected:
  void SetUp() final {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  const HeapVector<Member<Node>>& GetFlatTreeChildren(HTMLSlotElement& slot) {
    // Recomputes the flat tree children if the assignment is dirty.
    slot.AssignedNodes();
    return slot.flat_tree_children_;
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(HTMLSlotElementInDocumentTest, RecalcAssignedNodeStyleForReattach) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id='host'><span id='span'></span></div>
  )HTML");

  Element& host = *GetDocument().getElementById(AtomicString("host"));
  Element& span = *GetDocument().getElementById(AtomicString("span"));

  ShadowRoot& shadow_root =
      host.AttachShadowRootForTesting(ShadowRootMode::kOpen);

  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      R"HTML(<span><slot /></span>)HTML");

  auto* shadow_span = To<Element>(shadow_root.firstChild());
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  shadow_span->setAttribute(html_names::kStyleAttr,
                            AtomicString("display:block"));

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().GetStyleEngine().RecalcStyle();

  EXPECT_TRUE(shadow_span->GetComputedStyle());
  EXPECT_TRUE(span.GetComputedStyle());
}

TEST_F(HTMLSlotElementInDocumentTest, SlotableFallback) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id='host'></div>
  )HTML");

  Element& host = *GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host.AttachShadowRootForTesting(ShadowRootMode::kOpen);

  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      R"HTML(<slot><span></span><!-- -->text</slot>)HTML");

  auto* slot = To<HTMLSlotElement>(shadow_root.firstChild());

  EXPECT_TRUE(slot->AssignedNodes().empty());
  EXPECT_EQ(2u, GetFlatTreeChildren(*slot).size());
}

TEST_F(HTMLSlotElementInDocumentTest,
       MoveElementBetweenShadowHostAndSlotInvalidatesStyle) {
  // Avoid whitespace as direct children of the shadow hosts, since whitespace
  // text nodes would be assigned to the unnamed <slot> and prevent fallback.
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id="host1"><template shadowrootmode="open">
      <style>
        ::slotted(div) { color: rgb(0, 128, 0); }
        slot > div { color: rgb(0, 0, 255); }
      </style>
      <slot id="slot1"></slot>
    </template><div id="slotted1"></div></div>
    <div id="host2"><template shadowrootmode="open">
      <style>
        ::slotted(div) { color: rgb(0, 128, 0); }
        slot > div { color: rgb(0, 0, 255); }
      </style>
      <slot id="slot2"></slot>
    </template><div id="slotted2"></div></div>
    <div id="host3"><template shadowrootmode="open">
      <style>
        ::slotted(div) { color: rgb(0, 128, 0); }
        slot > div { color: rgb(0, 0, 255); }
      </style>
      <slot id="slot3">
        <div id="fallback1"></div>
      </slot>
    </template></div>
    <div id="host4"><template shadowrootmode="open">
      <style>
        ::slotted(div) { color: rgb(0, 128, 0); }
        slot > div { color: rgb(0, 0, 255); }
      </style>
      <slot id="slot4">
        <div id="fallback2"></div>
      </slot>
    </template></div>
    <div id="other"></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  constexpr Color kAssignedColor = Color::FromRGB(0, 128, 0);
  constexpr Color kFallbackColor = Color::FromRGB(0, 0, 255);

  auto* other = GetDocument().getElementById(AtomicString("other"));
  auto* host1 = GetDocument().getElementById(AtomicString("host1"));
  auto* slotted1 = GetDocument().getElementById(AtomicString("slotted1"));
  auto* slot1 = host1->GetShadowRoot()->getElementById(AtomicString("slot1"));

  auto* host2 = GetDocument().getElementById(AtomicString("host2"));
  auto* slotted2 = GetDocument().getElementById(AtomicString("slotted2"));
  auto* slot2 = host2->GetShadowRoot()->getElementById(AtomicString("slot2"));

  auto* host3 = GetDocument().getElementById(AtomicString("host3"));
  auto* slot3 = host3->GetShadowRoot()->getElementById(AtomicString("slot3"));
  auto* fallback1 =
      host3->GetShadowRoot()->getElementById(AtomicString("fallback1"));

  auto* host4 = GetDocument().getElementById(AtomicString("host4"));
  auto* slot4 = host4->GetShadowRoot()->getElementById(AtomicString("slot4"));
  auto* fallback2 =
      host4->GetShadowRoot()->getElementById(AtomicString("fallback2"));

  auto color_of = [](const Element* element) {
    return element->GetComputedStyle()->VisitedDependentColor(
        GetCSSPropertyColor());
  };

  ASSERT_EQ(FlatTreeTraversal::Parent(*slotted1), slot1);
  ASSERT_EQ(FlatTreeTraversal::Parent(*slotted2), slot2);
  ASSERT_EQ(FlatTreeTraversal::Parent(*fallback1), slot3);
  ASSERT_EQ(FlatTreeTraversal::Parent(*fallback2), slot4);
  ASSERT_TRUE(slotted1->GetComputedStyle());
  ASSERT_TRUE(slotted2->GetComputedStyle());
  ASSERT_TRUE(fallback1->GetComputedStyle());
  ASSERT_TRUE(fallback2->GetComputedStyle());
  EXPECT_EQ(color_of(slotted1), kAssignedColor);
  EXPECT_EQ(color_of(slotted2), kAssignedColor);
  EXPECT_EQ(color_of(fallback1), kFallbackColor);
  EXPECT_EQ(color_of(fallback2), kFallbackColor);

  // 1. Assigned node -> slot fallback content (via appendChild and moveBefore),
  //    and slot fallback content -> assigned node (via appendChild and
  //    moveBefore).
  slot1->appendChild(slotted1);
  slot2->moveBefore(slotted2, nullptr, ASSERT_NO_EXCEPTION);
  host3->appendChild(fallback1);
  host4->moveBefore(fallback2, nullptr, ASSERT_NO_EXCEPTION);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(FlatTreeTraversal::Parent(*slotted1), slot1);
  ASSERT_TRUE(slotted1->GetComputedStyle());
  EXPECT_TRUE(slotted1->GetLayoutObject());
  EXPECT_EQ(color_of(slotted1), kFallbackColor);

  EXPECT_EQ(FlatTreeTraversal::Parent(*slotted2), slot2);
  ASSERT_TRUE(slotted2->GetComputedStyle());
  EXPECT_TRUE(slotted2->GetLayoutObject());
  EXPECT_EQ(color_of(slotted2), kFallbackColor);

  EXPECT_EQ(FlatTreeTraversal::Parent(*fallback1), slot3);
  ASSERT_TRUE(fallback1->GetComputedStyle());
  EXPECT_TRUE(fallback1->GetLayoutObject());
  EXPECT_EQ(color_of(fallback1), kAssignedColor);

  EXPECT_EQ(FlatTreeTraversal::Parent(*fallback2), slot4);
  ASSERT_TRUE(fallback2->GetComputedStyle());
  EXPECT_TRUE(fallback2->GetLayoutObject());
  EXPECT_EQ(color_of(fallback2), kAssignedColor);

  // 2. Move each element back in the opposite direction.
  host1->appendChild(slotted1);
  host2->moveBefore(slotted2, nullptr, ASSERT_NO_EXCEPTION);
  slot3->appendChild(fallback1);
  slot4->moveBefore(fallback2, nullptr, ASSERT_NO_EXCEPTION);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(FlatTreeTraversal::Parent(*slotted1), slot1);
  ASSERT_TRUE(slotted1->GetComputedStyle());
  EXPECT_TRUE(slotted1->GetLayoutObject());
  EXPECT_EQ(color_of(slotted1), kAssignedColor);

  EXPECT_EQ(FlatTreeTraversal::Parent(*slotted2), slot2);
  ASSERT_TRUE(slotted2->GetComputedStyle());
  EXPECT_TRUE(slotted2->GetLayoutObject());
  EXPECT_EQ(color_of(slotted2), kAssignedColor);

  EXPECT_EQ(FlatTreeTraversal::Parent(*fallback1), slot3);
  ASSERT_TRUE(fallback1->GetComputedStyle());
  EXPECT_TRUE(fallback1->GetLayoutObject());
  EXPECT_EQ(color_of(fallback1), kFallbackColor);

  EXPECT_EQ(FlatTreeTraversal::Parent(*fallback2), slot4);
  ASSERT_TRUE(fallback2->GetComputedStyle());
  EXPECT_TRUE(fallback2->GetLayoutObject());
  EXPECT_EQ(color_of(fallback2), kFallbackColor);

  // 3. Detour an assigned node through an unrelated parent and back into the
  //    same shadow host in a single task (via appendChild and moveBefore).
  other->appendChild(slotted1);
  host1->appendChild(slotted1);

  other->moveBefore(slotted2, nullptr, ASSERT_NO_EXCEPTION);
  host2->moveBefore(slotted2, nullptr, ASSERT_NO_EXCEPTION);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(FlatTreeTraversal::Parent(*slotted1), slot1);
  ASSERT_TRUE(slotted1->GetComputedStyle());
  EXPECT_TRUE(slotted1->GetLayoutObject());
  EXPECT_EQ(color_of(slotted1), kAssignedColor);

  EXPECT_EQ(FlatTreeTraversal::Parent(*slotted2), slot2);
  ASSERT_TRUE(slotted2->GetComputedStyle());
  EXPECT_TRUE(slotted2->GetLayoutObject());
  EXPECT_EQ(color_of(slotted2), kAssignedColor);
}

// A rendered element moved with moveBefore() into a slot that already has
// assigned nodes keeps its layout object across the move. It is inactive
// fallback content, so slot assignment recalc must detach it, even though the
// slot did not switch between assigned nodes and fallback.
TEST_F(HTMLSlotElementInDocumentTest, MoveBeforeIntoSlotWithAssignedNodes) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id="host">
      <template shadowrootmode="open"><slot id="slot"></slot></template>
      <span id="assigned">assigned</span>
    </div>
    <div id="other"><span id="moved">moved</span></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Element* host = GetDocument().getElementById(AtomicString("host"));
  auto* slot = To<HTMLSlotElement>(
      host->GetShadowRoot()->getElementById(AtomicString("slot")));
  Element* moved = GetDocument().getElementById(AtomicString("moved"));
  ASSERT_FALSE(slot->AssignedNodes().empty());
  ASSERT_TRUE(moved->GetLayoutObject());

  slot->moveBefore(moved, nullptr, ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(slot->AssignedNodes().empty());
  EXPECT_EQ(FlatTreeTraversal::Parent(*moved), nullptr);
  EXPECT_FALSE(moved->GetLayoutObject());
}

// A slot moved with moveBefore() out of a shadow tree renders its children.
// When it is then moved into another shadow tree where it gets assigned nodes,
// those children become inactive fallback content and must be detached.
TEST_F(HTMLSlotElementInDocumentTest, MoveBeforeSlotBetweenTrees) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id="host1">
      <template shadowrootmode="open"><div id="container"><slot id="slot"><span
          id="fallback">fallback</span></slot></div></template>
      <span>assigned1</span>
    </div>
    <div id="other"></div>
    <div id="host2">
      <template shadowrootmode="open"><div id="container"></div></template>
      <span>assigned2</span>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  Element* host1 = GetDocument().getElementById(AtomicString("host1"));
  Element* host2 = GetDocument().getElementById(AtomicString("host2"));
  Element* other = GetDocument().getElementById(AtomicString("other"));
  auto* slot = To<HTMLSlotElement>(
      host1->GetShadowRoot()->getElementById(AtomicString("slot")));
  Element* fallback =
      host1->GetShadowRoot()->getElementById(AtomicString("fallback"));
  Element* container2 =
      host2->GetShadowRoot()->getElementById(AtomicString("container"));
  ASSERT_FALSE(slot->AssignedNodes().empty());
  ASSERT_FALSE(fallback->GetLayoutObject());

  // Outside a shadow tree, the slot renders its children.
  other->moveBefore(slot, nullptr, ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(fallback->GetLayoutObject());

  container2->moveBefore(slot, nullptr, ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(slot->AssignedNodes().empty());
  EXPECT_EQ(FlatTreeTraversal::Parent(*fallback), nullptr);
  EXPECT_FALSE(fallback->GetLayoutObject());
}

}  // namespace blink
