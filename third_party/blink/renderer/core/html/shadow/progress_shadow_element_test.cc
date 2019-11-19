// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/shadow/progress_shadow_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class ProgressShadowElementTest : public testing::Test {
 protected:
  void SetUp() final {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  }
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(ProgressShadowElementTest, LayoutObjectIsNeeded) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <progress id='prog' style='-webkit-appearance:none' />
  )HTML");

  auto* progress =
      To<HTMLProgressElement>(GetDocument().getElementById("prog"));
  ASSERT_TRUE(progress);

  auto* shadow_element = To<Element>(progress->GetShadowRoot()->firstChild());
  ASSERT_TRUE(shadow_element);

  GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);

  progress->SetForceReattachLayoutTree();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().GetStyleEngine().RecalcStyle();
  EXPECT_TRUE(shadow_element->GetComputedStyle());

  scoped_refptr<ComputedStyle> style = shadow_element->StyleForLayoutObject();
  EXPECT_TRUE(shadow_element->LayoutObjectIsNeeded(*style));
}

}  // namespace blink
