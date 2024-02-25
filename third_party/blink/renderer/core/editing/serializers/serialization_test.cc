// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/serializers/serialization.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

// See third_party/googletest/src/googletest/docs/advanced.md for supported
// regexp operators.
using ::testing::MatchesRegex;

class SerializationTest : public EditingTestBase {
 protected:
  std::string SerailizeToHTMLText(const Node& node) {
    // We use same |CreateMarkupOptions| used in
    // |FrameSelection::SelectedHTMLForClipboard()|
    return CreateMarkup(Position::BeforeNode(node), Position::AfterNode(node),
                        CreateMarkupOptions::Builder()
                            .SetShouldAnnotateForInterchange(true)
                            .SetShouldResolveURLs(kResolveNonLocalURLs)
                            .Build())
        .Utf8();
  }
};

// Regression test for https://crbug.com/1032673
TEST_F(SerializationTest, CantCreateFragmentCrash) {
  // CreateFragmentFromMarkupWithContext() fails to create a fragment for the
  // following markup. Should return nullptr as the strictly processed fragment
  // instead of crashing.
  const String html =
      "<article><dcell></dcell>A<td><dcol></"
      "dcol>A0<td>&percnt;&lbrack;<command></"
      "command><img>0AA00A0AAAAAAA00A<optgroup>&NotLess;&Eacute;&andand;&"
      "Uarrocir;&jfr;&esim;&Alpha;&angmsdab;&ogt;&lesseqqgtr;&vBar;&plankv;&"
      "curlywedge;&lcedil;&Mfr;&Barwed;&rlm;<kbd><animateColor></"
      "animateColor>A000AA0AA000A0<plaintext></"
      "plaintext><title>0A0AA00A0A0AA000A<switch><img "
      "src=\"../resources/abe.png\"> zz";
  DocumentFragment* strictly_processed_fragment =
      CreateStrictlyProcessedFragmentFromMarkupWithContext(
          GetDocument(), html, 0, html.length(), KURL());
  EXPECT_FALSE(strictly_processed_fragment);
}

// Regression test for https://crbug.com/1310535
TEST_F(SerializationTest, CreateFragmentWithDataUrlCrash) {
  // When same data: URL is set for filter and style image with a style element
  // CreateStrictlyProcessedFragmentFromMarkupWithContext() triggers
  // ResourceLoader::Start(), and EmptyLocalFrameClientWithFailingLoaderFactory
  // ::CreateURLLoaderFactory() will be called.
  // Note: Ideally ResourceLoader::Start() don't need to call
  // EmptyLocalFrameClientWithFailingLoaderFactory::CreateURLLoaderFactory() for
  // data: URL.
  const String html =
      "<div style=\"filter: url(data:image/gif;base64,xx);\">"
      "<style>body {background: url(data:image/gif;base64,xx);}</style>";
  DocumentFragment* strictly_processed_fragment =
      CreateStrictlyProcessedFragmentFromMarkupWithContext(
          GetDocument(), html, 0, html.length(), KURL());
  EXPECT_TRUE(strictly_processed_fragment);
}

// http://crbug.com/938590
TEST_F(SerializationTest, Link) {
  InsertStyleElement(
      "a { color: #010101; }"
      "a:link { color: #020202; }"
      "a:visited { color: #030303; }");
  SetBodyContent(
      "<a id=a1>text</a>"
      "<a id=a2 href=''>visited</a>"
      "<a id=a3 href='https://1.1.1.1/'>unvisited</a>");

  const auto& a1 = *GetElementById("a1");
  const auto& style1 = a1.ComputedStyleRef();
  const auto& a2 = *GetElementById("a2");
  const auto& style2 = a2.ComputedStyleRef();
  const auto& a3 = *GetElementById("a3");
  const auto& style3 = a3.ComputedStyleRef();

  // a1
  ASSERT_THAT(style1.InsideLink(), EInsideLink::kNotInsideLink);
  ASSERT_THAT(style1.VisitedDependentColor(GetCSSPropertyColor()),
              Color::FromRGB(1, 1, 1))
      << "should not be :visited/:link color";
  EXPECT_THAT(
      SerailizeToHTMLText(a1),
      MatchesRegex(
          R"re(<a id="a1" style=".*;? ?color: rgb\(1, 1, 1\);.*">text</a>)re"));

  // a2
  // Note: Because href="" means current document URI, it is visited.
  // We should have :link color instead of :visited color not to expose
  // visited/unvisited state of link for privacy reason.
  ASSERT_THAT(style2.InsideLink(), EInsideLink::kInsideVisitedLink);
  ASSERT_THAT(style2.VisitedDependentColor(GetCSSPropertyColor()),
              Color::FromRGB(3, 3, 3))
      << "should be :visited color";
  EXPECT_THAT(
      SerailizeToHTMLText(a2),
      MatchesRegex(
          R"re(<a id="a2" href="" style=".*;? ?color: rgb\(2, 2, 2\);.*">visited</a>)re"));

  // a3
  ASSERT_THAT(style3.InsideLink(), EInsideLink::kInsideUnvisitedLink);
  ASSERT_THAT(style3.VisitedDependentColor(GetCSSPropertyColor()),
              Color::FromRGB(2, 2, 2))
      << "should be :link color";
  EXPECT_THAT(
      SerailizeToHTMLText(a3),
      MatchesRegex(
          R"re(<a id="a3" href="https://1.1.1.1/" style=".*;? ?color: rgb\(2, 2, 2\);.*">unvisited</a>)re"));
}

// Regression test for https://crbug.com/1032389
TEST_F(SerializationTest, SVGForeignObjectCrash) {
  const String markup =
      "<svg>"
      "  <foreignObject>"
      "    <br>"
      "    <div style=\"height: 50px;\"></div>"
      "  </foreignObject>"
      "</svg>"
      "<span>\u00A0</span>";
  DocumentFragment* strictly_processed_fragment =
      CreateStrictlyProcessedFragmentFromMarkupWithContext(
          GetDocument(), markup, 0, markup.length(), KURL());
  // This is a crash test. We don't verify the content of the strictly processed
  // markup as it's too verbose and not interesting.
  EXPECT_TRUE(strictly_processed_fragment);
}

}  // namespace blink
