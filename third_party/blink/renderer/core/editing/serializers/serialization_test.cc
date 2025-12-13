// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/serializers/serialization.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/math_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

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

  String AsciiToItalicMathVariant(char ascii) {
    UChar32 math_char = blink::unicode::ItalicMathVariant(ascii);
    StringBuilder builder;
    builder.Append(math_char);
    return builder.ToString();
  }

  Element* GetFirstChildElementNamed(const Node& parent,
                                     const AtomicString& tag_name) {
    for (Node* child = parent.firstChild(); child;
         child = child->nextSibling()) {
      if (auto* element = DynamicTo<Element>(child)) {
        if (element->nodeName() == tag_name) {
          return element;
        }
      }
    }
    return nullptr;
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

// Regression test for https://crbug.com/464761431
// A test to confirm that a crash does not occur even if an external URL
// request is requested during serialization.
TEST_F(SerializationTest, CreateFragmentWithDataExternalUrlCrash) {
  const String html =
      "<link rel=\"stylesheet\" href=\"https://example.com/1\">"
      "<style></style>";
  DocumentFragment* strictly_processed_fragment =
      CreateStrictlyProcessedFragmentFromMarkupWithContext(
          GetDocument(), html, 0, html.length(), KURL());
  EXPECT_TRUE(strictly_processed_fragment);
}

// Regression test for https://crbug.com/464761431
// A test to confirm that SingleRequestURLLoaderFactory returned by
// EmptyLocalFrameClientWithFailingLoaderFactory is not used multiple times by
// mistake.
TEST_F(SerializationTest, CreateFragmentWithDataExternalUrlsCrash) {
  const String html =
      "<link rel=\"stylesheet\" href=\"https://example.com/1\">"
      "<link rel=\"stylesheet\" href=\"https://example.com/2\">"
      "<style></style>";
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

// Regression test for https://crbug.com/40840595
TEST_F(SerializationTest, CSSFontFaceLoadCrash) {
  const String markup =
      "<style>"
      "  @font-face {"
      "    font-family: \"custom-font\";"
      "    src: "
      "url(\"https://mdn.github.io/css-examples/web-fonts/VeraSeBd.ttf\");"
      "  }"
      "  </style>"
      "<span style=\"font-family: custom-font\">lorem ipsum</span>";
  const String sanitized_markup = CreateStrictlyProcessedMarkupWithContext(
      GetDocument(), markup, 0, markup.length(), KURL());
  // This is a crash test. We don't verify the content of the strictly processed
  // markup as it is not interesting.
  EXPECT_TRUE(sanitized_markup);
}

// Tests that the entire <math> element is serialized and deserialized with
// proper structure and namespace.
TEST_F(SerializationTest, MathML_EntireMathElement) {
  SetBodyContent(
      "<math xmlns='http://www.w3.org/1998/Math/MathML'>"
      "<mrow><mi>x</mi><mo>+</mo><mi>y</mi></mrow>"
      "</math>");
  const auto& original_math_element = *GetDocument().body()->firstChild();
  std::string serialized_markup = SerailizeToHTMLText(original_math_element);

  SetBodyContent(serialized_markup);

  const auto& math_root = *GetDocument().body()->firstChild();
  EXPECT_EQ(math_root.nodeName(), "math");
  EXPECT_TRUE(To<Element>(math_root).hasAttribute(AtomicString("xmlns")));
  EXPECT_EQ(To<Element>(math_root).getAttribute(AtomicString("xmlns")),
            mathml_names::kNamespaceURI);

  const auto* math_row =
      GetFirstChildElementNamed(math_root, AtomicString("mrow"));
  ASSERT_NE(math_row, nullptr) << "mrow element not found";
  EXPECT_EQ(math_row->parentNode(), &math_root);

  int mi_count = 0;
  bool found_mo = false;
  String mo_content;

  for (Node* child = math_row->firstChild(); child;
       child = child->nextSibling()) {
    const auto& name = child->nodeName();
    if (name == "mi") {
      ++mi_count;
    } else if (name == "mo") {
      found_mo = true;
      mo_content = child->textContent().SimplifyWhiteSpace();
    }
  }

  EXPECT_EQ(mi_count, 2);
  EXPECT_TRUE(found_mo);
  EXPECT_EQ(mo_content, "+");
}

// Tests that partial selection inside <math> preserves ancestor structure and
// serializes math italic characters.
TEST_F(SerializationTest, MathML_PartialSelectionInsideMath) {
  SetBodyContent(
      "<math xmlns='http://www.w3.org/1998/Math/MathML'>"
      "<mrow><mi>x</mi><mo>+</mo><mi>y</mi></mrow>"
      "<mrow><mi>a</mi><mo>+</mo><mi>b</mi></mrow>"
      "</math>");
  const auto& mrow = *GetDocument().body()->firstChild()->firstChild();
  const auto& mo = *mrow.firstChild()->nextSibling();
  const auto& mi_y = *mrow.lastChild();

  std::string serialized_markup =
      CreateMarkup(Position::BeforeNode(mo), Position::AfterNode(mi_y),
                   CreateMarkupOptions::Builder()
                       .SetShouldAnnotateForInterchange(true)
                       .Build())
          .Utf8();

  SetBodyContent(serialized_markup);

  const auto& math_root = *GetDocument().body()->firstChild();
  EXPECT_EQ(math_root.nodeName(), "math");
  EXPECT_TRUE(To<Element>(math_root).hasAttribute(AtomicString("xmlns")));
  EXPECT_EQ(To<Element>(math_root).getAttribute(AtomicString("xmlns")),
            mathml_names::kNamespaceURI);

  const auto* math_row =
      GetFirstChildElementNamed(math_root, AtomicString("mrow"));
  ASSERT_NE(math_row, nullptr) << "mrow element not found";

  // Verify that serialized markup contains only one mrow (from partial
  // selection)
  int mrow_count = 0;
  for (Node* child = math_root.firstChild(); child;
       child = child->nextSibling()) {
    if (child->nodeName() == "mrow") {
      ++mrow_count;
    }
  }
  EXPECT_EQ(mrow_count, 1)
      << "Should have exactly one mrow from partial selection";

  bool found_mo = false;
  bool found_mi = false;
  String mo_content, mi_content;

  for (Node* child = math_row->firstChild(); child;
       child = child->nextSibling()) {
    const auto& name = child->nodeName();
    if (name == "mo") {
      found_mo = true;
      mo_content = child->textContent().SimplifyWhiteSpace();
    } else if (name == "mi") {
      found_mi = true;
      mi_content = child->textContent().SimplifyWhiteSpace();
    }
  }

  EXPECT_TRUE(found_mo);
  EXPECT_EQ(mo_content, "+");
  EXPECT_TRUE(found_mi);
  EXPECT_EQ(mi_content, AsciiToItalicMathVariant('y'));
}

// Tests that <mfrac> with nested <msup> and <msub> elements preserves correct
// structure and semantic characters.
TEST_F(SerializationTest, MathML_FractionWithSuperscript) {
  SetBodyContent(
      "<math xmlns='http://www.w3.org/1998/Math/MathML'>"
      "<mfrac>"
      "<msup><mi>x</mi><mn>2</mn></msup>"
      "<msub><mi>y</mi><mn>1</mn></msub>"
      "</mfrac>"
      "</math>");
  const auto& math_root = *GetDocument().body()->firstChild();
  std::string serialized_markup = SerailizeToHTMLText(math_root);
  SetBodyContent(serialized_markup);

  const auto& parsed_math = *GetDocument().body()->firstChild();
  EXPECT_EQ(parsed_math.nodeName(), "math");

  const auto* math_fraction =
      GetFirstChildElementNamed(parsed_math, AtomicString("mfrac"));
  ASSERT_NE(math_fraction, nullptr) << "mfrac element not found";
  EXPECT_EQ(math_fraction->parentNode(), &parsed_math);

  Node* first_child = math_fraction->firstChild();
  Node* second_child = first_child ? first_child->nextSibling() : nullptr;

  ASSERT_TRUE(first_child && second_child);
  EXPECT_EQ(first_child->nodeName(), "msup");
  EXPECT_EQ(second_child->nodeName(), "msub");

  Node* msup_first = first_child->firstChild();
  Node* msup_second = msup_first ? msup_first->nextSibling() : nullptr;

  ASSERT_TRUE(msup_first && msup_second);
  EXPECT_EQ(msup_first->nodeName(), "mi");
  EXPECT_EQ(msup_second->nodeName(), "mn");
  EXPECT_EQ(msup_first->textContent().SimplifyWhiteSpace(),
            AsciiToItalicMathVariant('x'));
  EXPECT_EQ(msup_second->textContent().SimplifyWhiteSpace(), "2");

  Node* msub_first = second_child->firstChild();
  Node* msub_second = msub_first ? msub_first->nextSibling() : nullptr;

  ASSERT_TRUE(msub_first && msub_second);
  EXPECT_EQ(msub_first->nodeName(), "mi");
  EXPECT_EQ(msub_second->nodeName(), "mn");
  EXPECT_EQ(msub_first->textContent().SimplifyWhiteSpace(),
            AsciiToItalicMathVariant('y'));
  EXPECT_EQ(msub_second->textContent().SimplifyWhiteSpace(), "1");
}

// Tests that selection of <mtr> in a MathML table preserves full ancestor
// hierarchy and cell content.
TEST_F(SerializationTest, MathML_TableWithTextElements) {
  SetBodyContent(
      "<math xmlns='http://www.w3.org/1998/Math/MathML'>"
      "<mtable>"
      "<mtr>"
      "<mtd><mtext>sin</mtext></mtd>"
      "<mtd><ms>result</ms></mtd>"
      "</mtr>"
      "</mtable>"
      "</math>");
  const auto& mtable = *GetDocument().body()->firstChild()->firstChild();
  const auto& row = *mtable.firstChild();

  std::string serialized_markup =
      CreateMarkup(Position::BeforeNode(row), Position::AfterNode(row),
                   CreateMarkupOptions::Builder()
                       .SetShouldAnnotateForInterchange(true)
                       .Build())
          .Utf8();

  SetBodyContent(serialized_markup);

  const auto& math_root = *GetDocument().body()->firstChild();
  EXPECT_EQ(math_root.nodeName(), "math");

  const auto* math_table =
      GetFirstChildElementNamed(math_root, AtomicString("mtable"));
  ASSERT_NE(math_table, nullptr) << "mtable element not found";
  EXPECT_EQ(math_table->parentNode(), &math_root);

  const auto* table_row =
      GetFirstChildElementNamed(*math_table, AtomicString("mtr"));
  ASSERT_NE(table_row, nullptr) << "mtr element not found";
  EXPECT_EQ(table_row->parentNode(), math_table);

  Node* first_cell = table_row->firstChild();
  Node* second_cell = first_cell ? first_cell->nextSibling() : nullptr;

  ASSERT_TRUE(first_cell && second_cell);
  EXPECT_EQ(first_cell->nodeName(), "mtd");
  EXPECT_EQ(second_cell->nodeName(), "mtd");

  Node* mtext_element = first_cell->firstChild();
  ASSERT_TRUE(mtext_element);
  EXPECT_EQ(mtext_element->nodeName(), "mtext");
  EXPECT_EQ(mtext_element->textContent().SimplifyWhiteSpace(), "sin");

  Node* ms_element = second_cell->firstChild();
  ASSERT_TRUE(ms_element);
  EXPECT_EQ(ms_element->nodeName(), "ms");
  EXPECT_EQ(ms_element->textContent().SimplifyWhiteSpace(), "result");
}

}  // namespace blink
