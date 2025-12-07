// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <variant>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

bool IsCountedOnParsing(std::variant<WebFeature, WebDXFeature> feature,
                        String css) {
  auto holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = holder->GetDocument();
  document.documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<style id=style></style>");
  Element* style = document.getElementById(AtomicString("style"));
  CHECK(style);
  style->SetInnerHTMLWithoutTrustedTypes(css);
  document.View()->UpdateAllLifecyclePhasesForTest();
  if (WebFeature* web_feature = std::get_if<WebFeature>(&feature)) {
    return document.IsUseCounted(*web_feature);
  }
  return document.IsWebDXFeatureCounted(std::get<WebDXFeature>(feature));
}

}  // namespace

class StyleUseCounterTest : public testing::Test {
 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(StyleUseCounterTest, CSSFunctions) {
  WebFeature feature = WebFeature::kCSSFunctions;
  EXPECT_FALSE(IsCountedOnParsing(feature, "div {}"));
  EXPECT_FALSE(IsCountedOnParsing(feature, "@invalid {}"));
  EXPECT_FALSE(IsCountedOnParsing(feature, "@layer {}"));
  EXPECT_TRUE(IsCountedOnParsing(feature, "@function --f() {}"));
}

TEST_F(StyleUseCounterTest, CssIf) {
  WebDXFeature feature = WebDXFeature::kIf;
  EXPECT_FALSE(IsCountedOnParsing(feature, "div { top: var(--x); }"));
  EXPECT_FALSE(IsCountedOnParsing(feature, "div { top: 10px; }"));
  EXPECT_FALSE(IsCountedOnParsing(feature, "div { top: if(!!!); }"));
  EXPECT_TRUE(
      IsCountedOnParsing(feature, "div { top: if(style(--x: 10px): 20px); }"));
}

TEST_F(StyleUseCounterTest, ViewportUnitVariants) {
  WebDXFeature feature = WebDXFeature::kViewportUnitVariants;
  EXPECT_FALSE(IsCountedOnParsing(feature, "body { top: 10vh; }"));
  EXPECT_FALSE(IsCountedOnParsing(feature, "body { top: 10vi; }"));
  EXPECT_FALSE(IsCountedOnParsing(feature, "body { top: 10vmax; }"));
  EXPECT_TRUE(IsCountedOnParsing(feature, "body { top: 10svh; }"));
  EXPECT_TRUE(IsCountedOnParsing(feature, "body { top: 10lvi; }"));
  EXPECT_TRUE(IsCountedOnParsing(feature, "body { top: 10dvmax; }"));
}

}  // namespace blink
