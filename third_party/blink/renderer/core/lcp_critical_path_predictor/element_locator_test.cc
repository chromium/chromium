// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
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

    auto locator = element_locator::OfElement(*target);

    if (test_case.expected_locator_string) {
      String locator_string = element_locator::ToStringForTesting(locator);
      EXPECT_EQ(String(test_case.expected_locator_string), locator_string);
    }
  }
}

class TokenStreamMatcherTest : public ::testing::Test {
 public:
  struct Expectation {
    enum class Type { kStartTag, kEndTag } type = Type::kStartTag;
    const char* tag_name;
    const char* id_attr = nullptr;
    bool should_match = false;
  };
  static const auto kEndTag = Expectation::Type::kEndTag;

  void TestMatch(element_locator::TokenStreamMatcher& matcher,
                 const Vector<Expectation>& exps) {
    size_t i = 0;
    for (const Expectation& exp : exps) {
      SCOPED_TRACE(testing::Message() << "expectation index = " << i);
      AtomicString tag_name(exp.tag_name);
      EXPECT_TRUE(tag_name.Impl()->IsStatic());

      switch (exp.type) {
        case Expectation::Type::kStartTag: {
          HTMLToken token;
          {
            const char* c = exp.tag_name;
            token.BeginStartTag(static_cast<LChar>(*c++));
            for (; *c != 0; ++c) {
              token.AppendToName(static_cast<UChar>(*c));
            }
          }

          if (exp.id_attr) {
            token.AddNewAttribute('i');
            token.AppendToAttributeName('d');

            for (const char* c = exp.id_attr; *c != 0; ++c) {
              token.AppendToAttributeValue(static_cast<LChar>(*c));
            }
          }

          bool matched =
              matcher.ObserveStartTagAndReportMatch(tag_name.Impl(), token);
          EXPECT_EQ(matched, exp.should_match);
        } break;
        case Expectation::Type::kEndTag:
          matcher.ObserveEndTag(tag_name.Impl());
          break;
      }

      ++i;
    }
  }
};

TEST_F(TokenStreamMatcherTest, SingleId) {
  ElementLocator locator;
  auto* c = locator.add_components()->mutable_id();
  c->set_id_attr("target");

  element_locator::TokenStreamMatcher matcher({locator});
  Vector<Expectation> exps = {
      {.tag_name = "h1"},
      {.type = kEndTag, .tag_name = "h1"},
      {.tag_name = "p"},
      {.tag_name = "input"},
      {.tag_name = "img", .id_attr = "target", .should_match = true},
      {.type = kEndTag, .tag_name = "div"},
  };

  TestMatch(matcher, exps);
}

TEST_F(TokenStreamMatcherTest, SingleNth) {
  ElementLocator locator;
  auto* c = locator.add_components()->mutable_nth();
  c->set_tag_name("img");
  c->set_index(2);

  element_locator::TokenStreamMatcher matcher({locator});
  Vector<Expectation> exps = {
      {.tag_name = "div"},  {.tag_name = "img"},
      {.tag_name = "span"}, {.type = kEndTag, .tag_name = "span"},
      {.tag_name = "img"},  {.tag_name = "img", .should_match = true},
      {.tag_name = "img"},  {.type = kEndTag, .tag_name = "div"},
  };

  TestMatch(matcher, exps);
}

TEST_F(TokenStreamMatcherTest, CloseAPElement) {
  ElementLocator locator;
  auto* c0 = locator.add_components()->mutable_nth();
  c0->set_tag_name("img");
  c0->set_index(0);
  auto* c1 = locator.add_components()->mutable_nth();
  c1->set_tag_name("p");
  c1->set_index(2);
  auto* c2 = locator.add_components()->mutable_id();
  c2->set_id_attr("container");

  EXPECT_EQ(String("/img[0]/p[2]/#container"),
            element_locator::ToStringForTesting(locator));

  element_locator::TokenStreamMatcher matcher({locator});
  Vector<Expectation> exps = {

      {.tag_name = "div", .id_attr = "container"},
      {.tag_name = "p"},
      {.tag_name = "img"},
      {.tag_name = "p"},
      {.tag_name = "p"},
      {.tag_name = "img", .should_match = true},
      {.type = kEndTag, .tag_name = "div"}};

  TestMatch(matcher, exps);
}

TEST_F(TokenStreamMatcherTest, Complicated) {
  ElementLocator locator;
  auto* c0 = locator.add_components()->mutable_nth();
  c0->set_tag_name("img");
  c0->set_index(1);
  auto* c1 = locator.add_components()->mutable_nth();
  c1->set_tag_name("article");
  c1->set_index(2);
  auto* c2 = locator.add_components()->mutable_id();
  c2->set_id_attr("container");

  EXPECT_EQ(String("/img[1]/article[2]/#container"),
            element_locator::ToStringForTesting(locator));

  element_locator::TokenStreamMatcher matcher({locator});
  Vector<Expectation> exps = {
      {.tag_name = "section", .id_attr = "container"},
      {.tag_name = "article"},
      {.type = kEndTag, .tag_name = "article"},
      {.tag_name = "article"},
      {.type = kEndTag, .tag_name = "article"},
      {.tag_name = "article"},
      {.tag_name = "h2"},
      {.type = kEndTag, .tag_name = "h2"},
      {.tag_name = "img"},
      {.tag_name = "img", .should_match = true},
      {.type = kEndTag, .tag_name = "article"},
      {.tag_name = "article"},
      {.type = kEndTag, .tag_name = "article"},
      {.type = kEndTag, .tag_name = "section"},
  };

  TestMatch(matcher, exps);
}

TEST_F(TokenStreamMatcherTest, DontMatchNonImg) {
  ElementLocator locator;
  auto* c0 = locator.add_components()->mutable_nth();
  c0->set_tag_name("p");
  c0->set_index(2);
  auto* c1 = locator.add_components()->mutable_id();
  c1->set_id_attr("container");

  EXPECT_EQ(String("/p[2]/#container"),
            element_locator::ToStringForTesting(locator));

  element_locator::TokenStreamMatcher matcher({locator});
  Vector<Expectation> exps = {
      {.tag_name = "div", .id_attr = "container"},
      {.tag_name = "p"},
      {.tag_name = "img"},
      {.tag_name = "p"},
      {.tag_name = "p", .should_match = false},
      {.type = kEndTag, .tag_name = "div"},
  };

  TestMatch(matcher, exps);
}

}  // namespace blink
