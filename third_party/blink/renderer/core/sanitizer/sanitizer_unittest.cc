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

namespace blink {

class SanitizerTest : public PageTestBase {};

// Regression test for crbug.com/487863654.
TEST_F(SanitizerTest, SvgSetWithMultipleColons) {
  // Payload from crbug.com/487863654.
  const char* payload =
      R"X(<svg viewBox="0 0 240 80" xmlns:xlink="http://www.w3.org/1999/xlink"><a id="foo"><text x="20" y="20">click me</text></a><set href="#foo" attributeName="xlink:href:x" to="javascript:alert()"></set></svg>)X";
  SetBodyInnerHTML(payload);
  Sanitizer::CreateEmpty()->Sanitize(GetDocument().body(),
                                     Sanitizer::Mode::kSafe);
  String result = GetDocument().body()->GetInnerHTMLString();
  EXPECT_FALSE(result.contains("attributeName"));
}

}  // namespace blink
