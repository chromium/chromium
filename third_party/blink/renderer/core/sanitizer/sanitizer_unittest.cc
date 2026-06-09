// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class SanitizerTest : public PageTestBase,
                      public testing::WithParamInterface<bool> {};

// Parameterized on SplitQualifiedNameOnFirstColon.
INSTANTIATE_TEST_SUITE_P(All,
                         SanitizerTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "FirstColonSplit" : "Legacy";
                         });

// Regression test for crbug.com/487863654.
TEST_P(SanitizerTest, SvgSetWithMultipleColons) {
  const bool split_on_first_colon = GetParam();
  ScopedSplitQualifiedNameOnFirstColonForTest scoped_feature(
      split_on_first_colon);

  // Payload from crbug.com/487863654.
  const char* payload =
      R"X(<svg viewBox="0 0 240 80" xmlns:xlink="http://www.w3.org/1999/xlink"><a id="foo"><text x="20" y="20">click me</text></a><set href="#foo" attributeName="xlink:href:x" to="javascript:alert()"></set></svg>)X";
  SetBodyInnerHTML(payload);
  Sanitizer::CreateEmpty()->Sanitize(GetDocument().body(),
                                     Sanitizer::Mode::kSafe);

  if (split_on_first_colon) {
    // Splitting on the first colon, per the DOM "validate and extract"
    // algorithm, gives prefix "xlink" and local name "href:x", so the sanitizer
    // keeps the attribute. See https://crbug.com/490251709.
    Element* set = GetDocument().QuerySelector(AtomicString("set"));
    ASSERT_TRUE(set);
    EXPECT_EQ("xlink:href:x", set->getAttribute(AtomicString("attributeName")));
  } else {
    // Legacy branch: "xlink:href:x" resolves to xlink:href, so the sanitizer
    // strips the attribute.
    String result = GetDocument().body()->GetInnerHTMLString();
    EXPECT_FALSE(result.contains("attributeName"));
  }
}

}  // namespace blink
