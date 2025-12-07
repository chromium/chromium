// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/inner_text_builder.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

TEST(InnerTextBuilderTest, Basic) {
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();
  // ScopedNullExecutionContext execution_context;
  ASSERT_TRUE(helper.LocalMainFrame());
  frame_test_helpers::LoadHTMLString(
      helper.LocalMainFrame(),
      "<body>container<iframe id='x'>iframe</iframe>after</body>",
      url_test_helpers::ToKURL("http://foobar.com"));
  auto iframe = helper.LocalMainFrame()->GetDocument().GetElementById("x");
  ASSERT_FALSE(iframe.IsNull());
  mojom::blink::InnerTextParams params;
  params.node_id = iframe.GetDomNodeId();
  auto frame =
      InnerTextBuilder::Build(*helper.LocalMainFrame()->GetFrame(), params);
  ASSERT_TRUE(frame);
  ASSERT_EQ(4u, frame->segments.size());
  ASSERT_TRUE(frame->segments[0]->is_text());
  EXPECT_EQ("container", frame->segments[0]->get_text());
  ASSERT_TRUE(frame->segments[1]->is_node_location());
  ASSERT_TRUE(frame->segments[2]->is_frame());
  const mojom::blink::InnerTextFramePtr& child_frame =
      frame->segments[2]->get_frame();
  HTMLIFrameElement* html_iframe =
      DynamicTo<HTMLIFrameElement>(iframe.Unwrap<Node>());
  EXPECT_EQ(html_iframe->contentDocument()->GetFrame()->GetLocalFrameToken(),
            child_frame->token);
  EXPECT_TRUE(frame->segments[3]->is_text());
  EXPECT_EQ("after", frame->segments[3]->get_text());
}

TEST(InnerTextBuilderTest, MultiFrames) {
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper helper;
  std::string base_url("http://internal.test/");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "inner_text_test1.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "subframe-a.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "subframe-b.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "subframe-d.html");
  helper.InitializeAndLoad(base_url + "inner_text_test1.html");

  auto web_iframe_d =
      helper.LocalMainFrame()->GetDocument().GetElementById("d");
  ASSERT_FALSE(web_iframe_d.IsNull());
  HTMLIFrameElement* iframe_d =
      DynamicTo<HTMLIFrameElement>(web_iframe_d.Unwrap<Node>());
  ASSERT_TRUE(iframe_d);
  mojom::blink::InnerTextParams params;
  params.node_id = iframe_d->contentDocument()
                       ->getElementById(AtomicString("bold"))
                       ->GetDomNodeId();

  auto frame =
      InnerTextBuilder::Build(*helper.LocalMainFrame()->GetFrame(), params);
  ASSERT_TRUE(frame);

  ASSERT_EQ(7u, frame->segments.size());
  ASSERT_TRUE(frame->segments[0]->is_text());
  EXPECT_EQ("A", frame->segments[0]->get_text());
  ASSERT_TRUE(frame->segments[1]->is_frame());
  // NOTE: the nesting in this function is intended to indicate a child frame.
  // It is purely for readability.
  {
    const mojom::blink::InnerTextFramePtr& frame_1 =
        frame->segments[1]->get_frame();
    ASSERT_EQ(1u, frame_1->segments.size());
    ASSERT_TRUE(frame_1->segments[0]->is_text());
    EXPECT_EQ("a", frame_1->segments[0]->get_text());
  }
  ASSERT_TRUE(frame->segments[2]->is_text());
  EXPECT_EQ("B C", frame->segments[2]->get_text());
  ASSERT_TRUE(frame->segments[3]->is_frame());
  {
    const mojom::blink::InnerTextFramePtr& frame_2 =
        frame->segments[3]->get_frame();
    ASSERT_EQ(2u, frame_2->segments.size());
    EXPECT_EQ("b ", frame_2->segments[0]->get_text());
    ASSERT_TRUE(frame_2->segments[1]->is_frame());
    {
      const mojom::blink::InnerTextFramePtr& frame_2_1 =
          frame_2->segments[1]->get_frame();
      ASSERT_EQ(1u, frame_2_1->segments.size());
      ASSERT_TRUE(frame_2_1->segments[0]->is_text());
      EXPECT_EQ("a", frame_2_1->segments[0]->get_text());
    }
  }
  ASSERT_TRUE(frame->segments[4]->is_text());
  EXPECT_EQ("D E", frame->segments[4]->get_text());
  ASSERT_TRUE(frame->segments[5]->is_frame());
  {
    const mojom::blink::InnerTextFramePtr& frame_3 =
        frame->segments[5]->get_frame();
    ASSERT_EQ(3u, frame_3->segments.size());
    ASSERT_TRUE(frame_3->segments[0]->is_text());
    EXPECT_EQ("e", frame_3->segments[0]->get_text());
    EXPECT_TRUE(frame_3->segments[1]->is_node_location());
    ASSERT_TRUE(frame_3->segments[2]->is_text());
    EXPECT_EQ("hello", frame_3->segments[2]->get_text());
  }
  ASSERT_TRUE(frame->segments[6]->is_text());
  EXPECT_EQ("F", frame->segments[6]->get_text());
}

TEST(InnerTextBuilderTest, DifferentOrigin) {
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper helper;
  std::string base_url("http://internal.test/");
  url_test_helpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                  test::CoreTestDataPath(),
                                                  "inner_text_test2.html");
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8("http://different-host.com/"),
      test::CoreTestDataPath(), "subframe-a.html");
  helper.InitializeAndLoad(base_url + "inner_text_test2.html");

  auto frame =
      InnerTextBuilder::Build(*helper.LocalMainFrame()->GetFrame(), {});

  // The child frame should not be included as it's not same-origin.
  ASSERT_TRUE(frame);
  ASSERT_EQ(1u, frame->segments.size());
  ASSERT_TRUE(frame->segments[0]->is_text());
  EXPECT_EQ("XY", frame->segments[0]->get_text());
}

////////////////////////////////////////////////////////////////////////////////

void ExpectChunkerResult(int max_words_per_aggregate_passage,
                         bool greedily_aggregate_sibling_nodes,
                         int max_passages,
                         int min_words_per_passage,
                         const std::string& html,
                         const std::vector<String>& expected_passages) {
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();
  ASSERT_TRUE(helper.LocalMainFrame());
  frame_test_helpers::LoadHTMLString(
      helper.LocalMainFrame(), html,
      url_test_helpers::ToKURL("http://foobar.com"));
  ASSERT_TRUE(helper.LocalMainFrame());

  mojom::blink::InnerTextParams params;
  params.max_words_per_aggregate_passage = max_words_per_aggregate_passage;
  params.greedily_aggregate_sibling_nodes = greedily_aggregate_sibling_nodes;
  params.max_passages = max_passages;
  params.min_words_per_passage = min_words_per_passage;

  mojom::blink::InnerTextFramePtr frame = InnerTextPassagesBuilder::Build(
      *helper.LocalMainFrame()->GetFrame(), params);
  ASSERT_TRUE(frame);
  std::vector<String> result_passages;
  for (auto& segment : frame->segments) {
    ASSERT_TRUE(segment->is_text());
    result_passages.push_back(segment->get_text());
  }

  EXPECT_EQ(result_passages, expected_passages);
}

TEST(InnerTextBuilderTest, InnerTextPassagesChunksSingleTextBlock) {
  std::string html = "<p>Here is a paragraph.</p>";
  ExpectChunkerResult(10, false, 0, 0, html,
                      {
                          "Here is a paragraph.",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesHandlesEscapeCodes) {
  std::string html = "<p>Here&#39;s a paragraph.</p>";
  ExpectChunkerResult(10, false, 0, 0, html,
                      {
                          "Here's a paragraph.",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesHandlesUnicodeCharacters) {
  std::string html =
      "<p>Here is a "
      "\u2119\u212b\u213e\u212b\u210A\u213e\u212b\u2119\u210F.</p>";
  ExpectChunkerResult(10, false, 0, 0, html,
                      {
                          u"Here is a ℙÅℾÅℊℾÅℙℏ.",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesHandlesByteString) {
  std::string html =
      "<p>Here is a "
      "\xe2\x84\x99\xe2\x84\xab\xe2\x84\xbe\xe2\x84\xab\xe2\x84\x8a\xe2\x84\xbe"
      "\xe2\x84\xab\xe2\x84\x99\xe2\x84\x8f.</p>";
  ExpectChunkerResult(10, false, 0, 0, html,
                      {
                          u"Here is a ℙÅℾÅℊℾÅℙℏ.",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesStripsWhitespaceAroundNodeText) {
  std::string html = R"(
      <div>
        <p>     )"
                     "\t"
                     R"(Here is a paragraph.)"
                     "\n"
                     R"(And another.)"
                     "\n"
                     R"(

        </p>
        <p>)"
                     "\t\n"
                     R"(

        </p>
        <p>And more.
        </p>
      </div>
      )";
  ExpectChunkerResult(
      8, false, 0, 0, html,
      {
          // Note, the newline is included in whitespace simplification.
          "Here is a paragraph. And another. And more.",
      });

  // Additional testing of whitespace handling on edges. Here the word count
  // will exceed the limit and create two separate passages.
  EXPECT_EQ(String(" And more.").SimplifyWhiteSpace(), "And more.");
  EXPECT_EQ(String("And more. ").SimplifyWhiteSpace(), "And more.");
  EXPECT_EQ(String(" And  more. ").SimplifyWhiteSpace(), "And more.");
  ExpectChunkerResult(7, false, 0, 0, html,
                      {
                          "Here is a paragraph. And another.",
                          "And more.",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesHandlesEmptyDomElements) {
  std::string html = "<div><p></p></div>";
  ExpectChunkerResult(10, false, 0, 0, html, {});
}

TEST(InnerTextBuilderTest, InnerTextPassagesChunksMultipleHtmlBlocks) {
  std::string html = R"(
      <div>
        <div>First level one.
          <div>Second level one.
            <div>
              <p>Third level one.</p><p>Third level two.</p>
              <span>Third level three.</span>
            </div>
          </div>
        </div>
        <div>First level two.
        </div>
      </div>
  )";
  ExpectChunkerResult(
      10, false, 0, 0, html,
      {
          "First level one.",
          "Second level one.",
          "Third level one. Third level two. Third level three.",
          "First level two.",
      });
}

TEST(InnerTextBuilderTest,
     InnerTextPassagesIncludesNodesOverMaxAggregateChunkSize) {
  std::string html = R"(
      <div>
        <div>First level one.
          <div>Second level one.
            <div>
              <p>Third level one.</p><p>Third level two.</p>
              <span>Third level three but now it's over the max aggregate chunk size alone.</span>
            </div>
          </div>
        </div>
        <div>First level two.
        </div>
      </div>
  )";
  ExpectChunkerResult(10, false, 0, 0, html,
                      {
                          "First level one.",
                          "Second level one.",
                          "Third level one.",
                          "Third level two.",
                          "Third level three but now it's over the max "
                          "aggregate chunk size alone.",
                          "First level two.",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesJoinsSplitTextNodesWithinPTag) {
  std::string html = R"(
      <p>Paragraph one with
          <a>link</a>
          and more.
      </p>
  )";
  ExpectChunkerResult(10, false, 0, 0, html,
                      {
                          "Paragraph one with link and more.",
                      });
}

TEST(InnerTextBuilderTest,
     InnerTextPassagesDoesNotJoinSplitTextNodesWithinPTagWhenOverMax) {
  std::string html = R"(
      <p>Paragraph one with
          <a>link</a>
          and more.
      </p>
  )";
  ExpectChunkerResult(1, false, 0, 0, html,
                      {
                          "Paragraph one with",
                          "link",
                          "and more.",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesExcludesTextFromSomeHtmlTags) {
  std::string html = R"(
      <!DOCTYPE html>
      <html>
        <head>
          <title>Title</title>
          <style>.my-tag{display:none}</style>
        <head>
        <body>
          <script type="application/json">{"@context":"https://schema.org"}</script>
          <p><!-- A comment -->Paragraph</p>
        </body>
      </html>
  )";
  ExpectChunkerResult(10, false, 0, 0, html,
                      {
                          "Title Paragraph",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesGreedilyAggregatesSiblingNodes) {
  std::string html = R"(
      <div>
        <div>First level one.
          <div>Second level one.
            <div>
              <p>Third level one.</p>
              <p>Third level two.</p>
              <p>Third level three.
                <span>Fourth level one.</span>
              </p>
              <span>Third level four that's over the max aggregate chunk size alone.</span>
              <p>Third level five.</p>
              <p>Third level six.</p>
            </div>
          </div>
        </div>
        <div>First level two.
          <div>
            <p>Second level two that should be output alone.
            <p>Second level three.
          </div>
        </div>
      </div>
  )";
  ExpectChunkerResult(
      10, true, 0, 0, html,
      {
          "First level one.",
          "Second level one.",
          "Third level one. Third level two.",
          "Third level three. Fourth level one.",
          "Third level four that's over the max aggregate chunk size alone.",
          "Third level five. Third level six.",
          "First level two.",
          "Second level two that should be output alone.",
          "Second level three.",
      });
}

TEST(InnerTextBuilderTest,
     InnerTextPassagesDoesNotGreedilyAggregateAcrossSectionBreaks) {
  // The first div should all be combined into a single passage since under
  // max words. The second div is over max words so should be split, and
  // because of the <h2> tag, "Header two" should not be greedily combined with
  // "Paragraph three" and instead combines with "Paragraph four". The third
  // div is the same as the second except the header is changed to a paragraph,
  // allowing it ("Paragraph six") to be combined with "Paragraph five".
  std::string html = R"(
      <div>
        <p>Paragraph one with
          <a>link</a>
          and more.
        </p>
        <h2>Header one</h2>
        <p>Paragraph two.
      </div>
      <div>
        <p>Paragraph three with
          <a>link</a>
          and more.
        </p>
        <h2>Header two</h2>
        <p>Paragraph four that puts entire div over length.</p>
      </div>
      <div>
        <p>Paragraph five with
          <a>link</a>
          and more.
        </p>
        <p>Paragraph six.</p>
        <p>Paragraph seven that puts entire div over length.</p>
      </div>
  )";
  ExpectChunkerResult(
      10, true, 0, 0, html,
      {
          "Paragraph one with link and more. Header one Paragraph two.",
          "Paragraph three with link and more.",
          "Header two Paragraph four that puts entire div over length.",
          "Paragraph five with link and more. Paragraph six.",
          "Paragraph seven that puts entire div over length.",
      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesTrimsExtraPassages) {
  std::string html = R"(
      <p>paragraph 1</p>
      <p>paragraph 2</p>
      <p>paragraph 3</p>
  )";
  ExpectChunkerResult(3, false, 2, 0, html,
                      {
                          "paragraph 1",
                          "paragraph 2",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesSkipsPassagesTooShort) {
  std::string html = R"(
      <p>an arbitrarily long paragraph</p>
      <p>short paragraph</p>
      <p>another long paragraph</p>
  )";
  ExpectChunkerResult(3, false, 0, 3, html,
                      {
                          "an arbitrarily long paragraph",
                          "another long paragraph",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesDropsShortPassagesAtMaxPassage) {
  std::string html = R"(
      <p>foo bar baz</p>
      <p>foo</p>
      <p>bar</p>
      <p>foo bar baz</p>
  )";
  ExpectChunkerResult(3, false, 2, 3, html,
                      {
                          "foo bar baz",
                          "foo bar baz",
                      });
}

TEST(InnerTextBuilderTest, InnerTextPassagesExcludesSvgElements) {
  std::string html = R"(
      <body>
      <p>foo bar baz</p>
      <svg>
        <defs>defs text is excluded</defs>
        <style>style text is excluded</style>
        <script>script text is excluded</script>
        text within svg
      </svg>
      <p>foo bar baz</p>
      </body>
  )";
  ExpectChunkerResult(10, false, 10, 0, html,
                      {
                          "foo bar baz text within svg foo bar baz",
                      });
}

}  // namespace
}  // namespace blink
