// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_view.h"

#include "build/build_config.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/page/named_pages_mapper.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutViewTest : public RenderingTest {
 public:
  LayoutViewTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(LayoutViewTest, UpdateCountersLayout) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div.incX { counter-increment: x }
      div.incY { counter-increment: y }
      div::before { content: counter(y) }
    </style>
    <div id=inc></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* inc = GetDocument().getElementById("inc");

  inc->setAttribute("class", "incX");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());

  UpdateAllLifecyclePhasesForTest();
  inc->setAttribute("class", "incY");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(GetDocument().View()->NeedsLayout());
}

TEST_F(LayoutViewTest, DisplayNoneFrame) {
  SetBodyInnerHTML(R"HTML(
    <iframe id="iframe" style="display:none"></iframe>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* iframe = To<HTMLIFrameElement>(GetDocument().getElementById("iframe"));
  Document* frame_doc = iframe->contentDocument();
  ASSERT_TRUE(frame_doc);
  LayoutObject* view = frame_doc->GetLayoutView();
  ASSERT_TRUE(view);
  EXPECT_FALSE(view->CanHaveChildren());
  EXPECT_FALSE(frame_doc->documentElement()->GetComputedStyle());

  frame_doc->body()->setInnerHTML(R"HTML(
    <div id="div"></div>
  )HTML");

  EXPECT_FALSE(frame_doc->NeedsLayoutTreeUpdate());
}

TEST_F(LayoutViewTest, NamedPages) {
  ScopedNamedPagesForTest named_pages_enabler(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div:empty { height:10px; }
    </style>
    <!-- First page: -->
    <div></div>
    <!-- Second page: -->
    <div style="break-before:page;"></div>
    <!-- Third page: -->
    <div style="page:yksi;"></div>
    <!-- Fourth page: -->
    <div style="page:yksi;">
      <div style="page:yksi; break-before:page;"></div>
      <!-- Fifth page: -->
      <div style="page:yksi; break-before:page;"></div>
    </div>
    <!-- Sixth page: -->
    <div style="page:kaksi;"></div>
    <!-- Seventh page: -->
    <div style="page:maksitaksi;"></div>
    <!-- Eighth page: -->
    <div></div>
    <!-- Ninth page: -->
    <div style="page:yksi;"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  const LayoutView* view = GetDocument().GetLayoutView();
  ASSERT_TRUE(view);

  ScopedPrintContext print_context(&GetDocument().View()->GetFrame());
  print_context->BeginPrintMode(500, 500);
  const NamedPagesMapper* mapper = view->GetNamedPagesMapper();
  ASSERT_TRUE(mapper);

  EXPECT_EQ(mapper->NamedPageAtIndex(0), AtomicString());
  EXPECT_EQ(mapper->NamedPageAtIndex(1), AtomicString());
  EXPECT_EQ(mapper->NamedPageAtIndex(2), "yksi");
  EXPECT_EQ(mapper->NamedPageAtIndex(3), "yksi");
  EXPECT_EQ(mapper->NamedPageAtIndex(4), "yksi");
  EXPECT_EQ(mapper->NamedPageAtIndex(5), "kaksi");
  EXPECT_EQ(mapper->NamedPageAtIndex(6), "maksitaksi");
  EXPECT_EQ(mapper->NamedPageAtIndex(7), AtomicString());
  EXPECT_EQ(mapper->NamedPageAtIndex(8), "yksi");
  EXPECT_EQ(mapper->NamedPageAtIndex(9), "yksi");
  EXPECT_EQ(mapper->NamedPageAtIndex(100), "yksi");
  EXPECT_EQ(mapper->LastPageName(), "yksi");
}

struct HitTestConfig {
  bool layout_ng;
  mojom::EditingBehavior editing_behavior;
};

class LayoutViewHitTestTest : public testing::WithParamInterface<HitTestConfig>,
                              private ScopedLayoutNGForTest,
                              public RenderingTest {
 public:
  LayoutViewHitTestTest()
      : ScopedLayoutNGForTest(GetParam().layout_ng),
        RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  bool LayoutNG() { return RuntimeEnabledFeatures::LayoutNGEnabled(); }
  bool IsAndroidOrWindowsEditingBehavior() {
    return GetParam().editing_behavior ==
               mojom::EditingBehavior::kEditingAndroidBehavior ||
           GetParam().editing_behavior ==
               mojom::EditingBehavior::kEditingWindowsBehavior;
  }

  void SetUp() override {
    RenderingTest::SetUp();
    GetFrame().GetSettings()->SetEditingBehaviorType(
        GetParam().editing_behavior);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LayoutViewHitTestTest,
    ::testing::Values(
        // Legacy
        HitTestConfig{false, mojom::EditingBehavior::kEditingMacBehavior},
        HitTestConfig{false, mojom::EditingBehavior::kEditingWindowsBehavior},
        HitTestConfig{false, mojom::EditingBehavior::kEditingUnixBehavior},
        HitTestConfig{false, mojom::EditingBehavior::kEditingAndroidBehavior},
        HitTestConfig{false, mojom::EditingBehavior::kEditingChromeOSBehavior},
        // LayoutNG
        HitTestConfig{true, mojom::EditingBehavior::kEditingMacBehavior},
        HitTestConfig{true, mojom::EditingBehavior::kEditingWindowsBehavior},
        HitTestConfig{true, mojom::EditingBehavior::kEditingUnixBehavior},
        HitTestConfig{true, mojom::EditingBehavior::kEditingAndroidBehavior},
        HitTestConfig{true, mojom::EditingBehavior::kEditingChromeOSBehavior}));

TEST_P(LayoutViewHitTestTest, HitTestHorizontal) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id="div" style="position: relative; font: 10px/10px Ahem;
        top: 100px; left: 50px; width: 200px; height: 80px">
      <span id="span1">ABCDE</span><span id="span2"
          style="position: relative; top: 30px">XYZ</span>
    </div>
  )HTML");

  // (50, 100)         (250, 100)
  //   |------------------|
  //   |ABCDE             |
  //   |                  |
  //   |                  |
  //   |     XYZ          |
  //   |                  |
  //   |                  |
  //   |------------------|
  // (50, 180)         (250, 180)
  auto* div = GetDocument().getElementById("div");
  auto* text1 = GetDocument().getElementById("span1")->firstChild();
  auto* text2 = GetDocument().getElementById("span2")->firstChild();

  HitTestResult result;
  // In body, but not in any descendants.
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(1, 1)), result);
  EXPECT_EQ(GetDocument().body(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-left corner of div and span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-right corner (outside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(251, 101)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(251, 101), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream),
            result.GetPosition());

  // Top-right corner (inside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(249, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(199, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream),
            result.GetPosition());

  // Top-right corner (inside) of span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(99, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(49, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 5), TextAffinity::kUpstream),
            result.GetPosition());

  // Top-right corner (outside) of span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(101, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(51, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Bottom-left corner (outside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 181)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(51, 181), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Bottom-left corner (inside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 179)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 79), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Bottom-left corner (outside) of span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 111)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 11), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-left corner of span2.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(101, 131)), result);
  EXPECT_EQ(text2, result.InnerNode());
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    EXPECT_EQ(PhysicalOffset(51, 31), result.LocalPoint());
  else
    EXPECT_EQ(PhysicalOffset(51, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());
}

TEST_P(LayoutViewHitTestTest, HitTestVerticalLR) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id="div" style="position: relative; font: 10px/10px Ahem;
        top: 100px; left: 50px; width: 200px; height: 80px;
        writing-mode: vertical-lr">
      <span id="span1">ABCDE</span><span id="span2"
          style="position: relative; left: 30px">XYZ</span>
    </div>
  )HTML");

  // (50, 100)         (250, 100)
  //   |------------------|
  //   |A                 |
  //   |B                 |
  //   |C                 |
  //   |D                 |
  //   |E                 |
  //   |   X              |
  //   |   Y              |
  //   |   Z              |
  //   |------------------|
  // (50, 180)         (250, 180)
  auto* div = GetDocument().getElementById("div");
  auto* text1 = GetDocument().getElementById("span1")->firstChild();
  auto* text2 = GetDocument().getElementById("span2")->firstChild();

  HitTestResult result;
  // In body, but not in any descendants.
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(1, 1)), result);
  EXPECT_EQ(GetDocument().body(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-left corner of div and span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-right corner (outside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(251, 101)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(251, 101), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-right corner (inside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(249, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(199, 1), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-right corner (inside) of span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(59, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(9, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-right corner (outside) of span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(61, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(11, 1), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Bottom-left corner (outside) of span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 181)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(51, 181), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream),
            result.GetPosition());

  // Bottom-left corner (inside) of span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 179)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 79), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream),
            result.GetPosition());

  // Top-left corner of span2.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(81, 151)), result);
  EXPECT_EQ(text2, result.InnerNode());
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    EXPECT_EQ(PhysicalOffset(31, 51), result.LocalPoint());
  else
    EXPECT_EQ(PhysicalOffset(1, 51), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());
}

TEST_P(LayoutViewHitTestTest, HitTestVerticalRL) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id="div" style="position: relative; font: 10px/10px Ahem;
        top: 100px; left: 50px; width: 200px; height: 80px;
        writing-mode: vertical-rl">
      <span id="span1">ABCDE</span><span id="span2"
          style="position: relative; left: -30px">XYZ</span>
    </div>
  )HTML");

  // (50, 100)         (250, 100)
  //   |------------------|
  //   |                 A|
  //   |                 B|
  //   |                 C|
  //   |                 D|
  //   |                 E|
  //   |              X   |
  //   |              Y   |
  //   |              Z   |
  //   |------------------|
  // (50, 180)         (250, 180)
  auto* div = GetDocument().getElementById("div");
  auto* text1 = GetDocument().getElementById("span1")->firstChild();
  auto* text2 = GetDocument().getElementById("span2")->firstChild();

  HitTestResult result;
  // In body, but not in any descendants.
  // XXX1
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(1, 1)), result);
  EXPECT_EQ(GetDocument().body(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 1), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-left corner of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 1), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-right corner (outside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(251, 101)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(251, 101), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-right corner (inside) of div and span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(249, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(199, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Bottom-right corner (inside) of span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(249, 149)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(199, 49), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 5), TextAffinity::kUpstream),
            result.GetPosition());

  // Bottom-right corner (outside) of span1 but inside of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(249, 151)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(199, 51), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Bottom-left corner (outside) of div.
  // XXX2
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 181)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(51, 181), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Bottom-left corner (inside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(51, 179)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 79), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Bottom-left corner (outside) of span1.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(241, 151)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(191, 51), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-right corner (inside) of span2.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(219, 151)), result);
  EXPECT_EQ(text2, result.InnerNode());
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    EXPECT_EQ(PhysicalOffset(169, 51), result.LocalPoint());
  else
    EXPECT_EQ(PhysicalOffset(199, 51), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());
}

TEST_P(LayoutViewHitTestTest, HitTestVerticalRLRoot) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      html { writing-mode: vertical-rl; }
      body { margin: 0 }
    </style>
    <div id="div" style="font: 10px/10px Ahem; width: 200px; height: 80px">
      <span id="span">ABCDE</span>
    </div>
  )HTML");

  // (0,0)     (600, 0)         (800, 0)
  // +----...----+---------------+
  // |           |              A|
  // |           |              B|
  // |           |              C|
  // |           |     (div)    D|
  // | (screen)  |              E|
  // |           |               |
  // |           |               |
  // |           +---------------+ (800, 80)
  // |       (600, 80)           |
  // .                           .
  // +----...--------------------+ (800, 600)

  auto* div = GetDocument().getElementById("div");
  auto* text = GetDocument().getElementById("span")->firstChild();
  HitTestResult result;
  // Not in any element. Should fallback to documentElement.
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(1, 1)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(-599, 1), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text, 5), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-left corner (inside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(601, 1)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(1, 1), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text, 5), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-right corner (outside) of div. Should fallback to documentElement.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(801, 1)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(201, 1), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text, 0), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-right corner (inside) of div and span.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(799, 1)), result);
  EXPECT_EQ(text, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(199, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Bottom-right corner (outside) of span1 but inside of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(799, 51)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(199, 51), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text, 5), TextAffinity::kUpstream),
            result.GetPosition());

  // Bottom-left corner (outside) of div.
  result = HitTestResult();
  GetLayoutView().HitTest(HitTestLocation(PhysicalOffset(599, 81)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(PhysicalOffset(-1, 81), result.LocalPoint());
  EXPECT_EQ(
      IsAndroidOrWindowsEditingBehavior()
          ? PositionWithAffinity(Position(text, 5), TextAffinity::kUpstream)
          : PositionWithAffinity(Position(text, 5), TextAffinity::kDownstream),
      result.GetPosition());
}

}  // namespace blink
