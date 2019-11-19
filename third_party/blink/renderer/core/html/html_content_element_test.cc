// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_content_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class HTMLContentElementTest : public testing::Test {
 protected:
  void SetUp() final {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  }
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(HTMLContentElementTest, FallbackRecalcForReattach) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <div id='host'></div>
  )HTML");

  Element* host = GetDocument().getElementById("host");
  ShadowRoot& root = host->CreateV0ShadowRootForTesting();
  GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);

  auto* content = GetDocument().CreateRawElement(html_names::kContentTag);
  auto* fallback = GetDocument().CreateRawElement(html_names::kSpanTag);
  content->AppendChild(fallback);
  root.AppendChild(content);

  GetDocument().UpdateDistributionForLegacyDistributedNodes();
  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  GetDocument().GetStyleEngine().RecalcStyle();

  EXPECT_TRUE(fallback->GetComputedStyle());
}

}  // namespace blink
