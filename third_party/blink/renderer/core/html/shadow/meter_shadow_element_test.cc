// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/shadow/meter_shadow_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_recalc_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class MeterShadowElementTest : public testing::Test {
 protected:
  void SetUp() final {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(MeterShadowElementTest, LayoutObjectIsNotNeeded) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <meter id='m' style='-webkit-appearance:none' />
  )HTML");

  auto* meter =
      To<HTMLMeterElement>(GetDocument().getElementById(AtomicString("m")));
  ASSERT_TRUE(meter);

  auto* shadow_element = To<Element>(meter->GetShadowRoot()->firstChild());
  EXPECT_TRUE(shadow_element);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  meter->SetForceReattachLayoutTree();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().GetStyleEngine().RecalcStyle();
  EXPECT_FALSE(shadow_element->GetComputedStyle());

  const ComputedStyle* style =
      shadow_element->StyleForLayoutObject(StyleRecalcContext());
  EXPECT_FALSE(shadow_element->LayoutObjectIsNeeded(*style));
}

TEST_F(MeterShadowElementTest, OnlyChangeDirectionOnShadowElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <meter id='m' style='writing-mode:vertical-lr; direction: ltr;' />
  )HTML");

  auto* meter =
      To<HTMLMeterElement>(GetDocument().getElementById(AtomicString("m")));
  ASSERT_TRUE(meter);

  auto* shadow_element = To<Element>(meter->GetShadowRoot()->firstChild());
  ASSERT_TRUE(shadow_element);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  meter->SetForceReattachLayoutTree();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().GetStyleEngine().RecalcStyle();

  EXPECT_TRUE(meter->GetComputedStyle());
  EXPECT_EQ(meter->GetComputedStyle()->Direction(), TextDirection::kLtr);

  EXPECT_TRUE(shadow_element->GetComputedStyle());
  EXPECT_EQ(shadow_element->GetComputedStyle()->Direction(),
            TextDirection::kRtl);
}

}  // namespace blink
