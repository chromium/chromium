// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.pb.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

using ElementLocatorTest = EditingTestBase;

bool HasDataLocateMe(Element& element) {
  DEFINE_STATIC_LOCAL(const AtomicString, kDataLocateMe, ("data-locate-me"));
  return element.hasAttribute(kDataLocateMe);
}

TEST_F(ElementLocatorTest, OfElement) {
  struct TestCase {
    const char* body_html;
    const char* expected_locator_string;
  };
  constexpr TestCase test_cases[] = {
      // Single element with an id.
      {"<div id='a' data-locate-me></div>", "/#a"},

      // No id on the element, so use relative position.
      {"<div id='container'><div data-locate-me></div></div>",
       "/div[0]/#container"},

      // No id on the document, so stop at BODY.
      {"<div data-locate-me></div>", "/div[0]/body[0]"},

      // Siblings
      {"<div id='container'><p><p><p><p data-locate-me><p></div>",
       "/p[3]/#container"},

      // Siblings with different tag names
      {"<div id='container'><h1></h1><p><p data-locate-me><p><a></a></div>",
       "/p[1]/#container"},

      // Misc complicated cases
      {"<section id='container'>"
       "<article></article>"
       "<article></article>"
       "<article><h2>Title</h2>"
       "  <img src=logo.svg>"
       "  <img src=photo.jpg data-locate-me>asdffdsa"
       "</article>"
       "<article></article>"
       "</section>",
       "/img[1]/article[2]/#container"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << std::endl
                                    << "body_html = " << test_case.body_html);

    SetBodyContent(test_case.body_html);
    Element* target =
        Traversal<Element>::FirstWithin(GetDocument(), HasDataLocateMe);
    ASSERT_TRUE(target);

    auto maybe_locator = element_locator::OfElement(target);

    if (test_case.expected_locator_string) {
      ASSERT_TRUE(maybe_locator.has_value());

      String locator_string = element_locator::ToString(maybe_locator.value());
      EXPECT_EQ(String(test_case.expected_locator_string), locator_string);
    }
  }
}

}  // namespace blink
