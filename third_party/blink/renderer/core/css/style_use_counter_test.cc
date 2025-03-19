// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

bool IsCountedOnParsing(WebFeature feature, String css) {
  auto holder = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = holder->GetDocument();
  document.documentElement()->setInnerHTML("<style id=style></style>");
  Element* style = document.getElementById(AtomicString("style"));
  CHECK(style);
  style->setInnerHTML(css);
  document.View()->UpdateAllLifecyclePhasesForTest();
  return document.IsUseCounted(feature);
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

}  // namespace blink
