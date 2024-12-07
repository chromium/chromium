// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

constexpr gfx::Size kWindowSize{1000, 1000};
constexpr char kSmallImage[] =
    "data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/"
    "2wBDAAMCAgICAgMCAgIDAwMDBAYEBAQEBAgGBgUGCQgKCgkICQkKDA8MCgsOCwkJDRENDg8QEB"
    "EQCgwSExIQEw8QEBD/"
    "2wBDAQMDAwQDBAgEBAgQCwkLEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEB"
    "AQEBAQEBAQEBAQEBD/wAARCAABAAEDASIAAhEBAxEB/"
    "8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/"
    "8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2J"
    "yggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDh"
    "IWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+"
    "Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/"
    "8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnL"
    "RChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6g"
    "oOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uP"
    "k5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwD+/iiiigD/2Q==";

class AIPageContentAgentTest : public testing::Test {
 public:
  AIPageContentAgentTest() = default;
  ~AIPageContentAgentTest() override = default;

  void SetUp() override {
    helper_.Initialize();
    helper_.Resize(kWindowSize);
    ASSERT_TRUE(helper_.LocalMainFrame());
  }

 protected:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(AIPageContentAgentTest, Basic) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    div {"
      "      position: fixed;"
      "      top: -10px;"
      "      left: -20px;"
      "    }"
      "  </style>"
      "  <div>text</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_TRUE(root.children_nodes.empty());

  const auto& attributes = *root.content_attributes;
  // One for root itself and one for the text content.
  EXPECT_EQ(attributes.dom_node_ids.size(), 2u);
  EXPECT_TRUE(attributes.common_ancestor_dom_node_id.has_value());

  EXPECT_EQ(attributes.attribute_type,
            mojom::blink::AIPageContentAttributeType::kRoot);

  ASSERT_TRUE(attributes.geometry);
  EXPECT_EQ(attributes.geometry->outer_bounding_box, gfx::Rect(kWindowSize));
  EXPECT_EQ(attributes.geometry->visible_bounding_box, gfx::Rect(kWindowSize));

  ASSERT_EQ(attributes.text_info.size(), 1u);
  const auto& text_info = *attributes.text_info[0];
  EXPECT_EQ(text_info.text_content, "text");
  EXPECT_EQ(text_info.text_bounding_box.x(), -20);
  EXPECT_EQ(text_info.text_bounding_box.y(), -10);
}

TEST_F(AIPageContentAgentTest, Image) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    img {"
      "      position: fixed;"
      "      top: -10px;"
      "      left: -20px;"
      "      width: 30px;"
      "      height: 40px;"
      "    }"
      "  </style>"
      "  <img alt=missing></img>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));
  auto& document = *helper_.LocalMainFrame()->GetFrame()->GetDocument();
  document.getElementsByTagName(AtomicString("img"))
      ->item(0)
      ->setAttribute(html_names::kSrcAttr, AtomicString(kSmallImage));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(document);
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_TRUE(root.children_nodes.empty());

  const auto& attributes = *root.content_attributes;
  // One for root itself and one for the image content.
  EXPECT_EQ(attributes.dom_node_ids.size(), 2u);

  ASSERT_EQ(attributes.image_info.size(), 1u);
  const auto& image_info = *attributes.image_info[0];
  EXPECT_EQ(image_info.image_caption, "missing");
  EXPECT_EQ(image_info.image_bounding_box, gfx::Rect(-20, -10, 30, 40));
}

TEST_F(AIPageContentAgentTest, ImageNoAltText) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      base::StringPrintf("<body>"
                         "  <style>"
                         "    div::before {"
                         "      content: url(%s);"
                         "    }"
                         "  </style>"
                         "  <div>text</div>"
                         "</body>",
                         kSmallImage),
      url_test_helpers::ToKURL("http://foobar.com"));
  auto& document = *helper_.LocalMainFrame()->GetFrame()->GetDocument();

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(document);
  auto page_content = agent->GetAIPageContentSync();

  mojom::blink::AIPageContentPtr output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::blink::AIPageContent>(
      page_content, output));
}

TEST_F(AIPageContentAgentTest, Headings) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <h1>Heading 1</h1>"
      "  <h2>Heading 2</h2>"
      "  <h3>Heading 3</h3>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 3u);

  const auto& heading1 = *root.children_nodes[0]->content_attributes;
  EXPECT_EQ(heading1.attribute_type,
            mojom::blink::AIPageContentAttributeType::kHeading);
  ASSERT_EQ(heading1.text_info.size(), 1u);
  EXPECT_EQ(heading1.text_info[0]->text_content, "Heading 1");

  const auto& heading2 = *root.children_nodes[1]->content_attributes;
  EXPECT_EQ(heading2.attribute_type,
            mojom::blink::AIPageContentAttributeType::kHeading);
  ASSERT_EQ(heading2.text_info.size(), 1u);
  EXPECT_EQ(heading2.text_info[0]->text_content, "Heading 2");

  const auto& heading3 = *root.children_nodes[2]->content_attributes;
  EXPECT_EQ(heading3.attribute_type,
            mojom::blink::AIPageContentAttributeType::kHeading);
  ASSERT_EQ(heading3.text_info.size(), 1u);
  EXPECT_EQ(heading3.text_info[0]->text_content, "Heading 3");
}

TEST_F(AIPageContentAgentTest, Paragraph) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    p {"
      "      position: fixed;"
      "      top: -10px;"
      "      left: -20px;"
      "      width: 200px;"
      "      height: 40px;"
      "      margin: 0;"
      "    }"
      "  </style>"
      "  <p>text inside paragraph</p>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& paragraph = *root.children_nodes[0]->content_attributes;
  EXPECT_EQ(paragraph.attribute_type,
            mojom::blink::AIPageContentAttributeType::kParagraph);
  EXPECT_EQ(paragraph.geometry->outer_bounding_box,
            gfx::Rect(-20, -10, 200, 40));
  EXPECT_EQ(paragraph.geometry->visible_bounding_box, gfx::Rect(0, 0, 180, 30));

  ASSERT_EQ(paragraph.text_info.size(), 1u);
  EXPECT_EQ(paragraph.text_info[0]->text_content, "text inside paragraph");
}

TEST_F(AIPageContentAgentTest, Lists) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <ul>"
      "    <li>Item 1</li>"
      "    <li>Item 2</li>"
      "  </ul>"
      "  <ol>"
      "    <li>Step 1</li>"
      "    <li>Step 2</li>"
      "  </ol>"
      "  <dl>"
      "    <dt>Detail 1 title</dt>"
      "    <dd>Detail 1 description</dd>"
      "    <dt>Detail 2 title</dt>"
      "    <dd>Detail 2 description</dd>"
      "  </dl>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 3u);

  const auto& ul = *root.children_nodes[0]->content_attributes;
  EXPECT_EQ(ul.attribute_type,
            mojom::blink::AIPageContentAttributeType::kUnorderedList);
  ASSERT_EQ(ul.text_info.size(), 2u);
  EXPECT_EQ(ul.text_info[0]->text_content, "Item 1");
  EXPECT_EQ(ul.text_info[1]->text_content, "Item 2");

  const auto& ol = *root.children_nodes[1]->content_attributes;
  EXPECT_EQ(ol.attribute_type,
            mojom::blink::AIPageContentAttributeType::kOrderedList);
  ASSERT_EQ(ol.text_info.size(), 2u);
  EXPECT_EQ(ol.text_info[0]->text_content, "Step 1");
  EXPECT_EQ(ol.text_info[1]->text_content, "Step 2");

  const auto& dl = *root.children_nodes[2]->content_attributes;
  EXPECT_EQ(dl.attribute_type,
            mojom::blink::AIPageContentAttributeType::kUnorderedList);
  ASSERT_EQ(dl.text_info.size(), 4u);
  EXPECT_EQ(dl.text_info[0]->text_content, "Detail 1 title");
  EXPECT_EQ(dl.text_info[1]->text_content, "Detail 1 description");
  EXPECT_EQ(dl.text_info[2]->text_content, "Detail 2 title");
  EXPECT_EQ(dl.text_info[3]->text_content, "Detail 2 description");
}

TEST_F(AIPageContentAgentTest, IFrameWithContent) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <iframe src='about:blank'></iframe>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* iframe_element =
      To<HTMLIFrameElement>(helper_.LocalMainFrame()
                                ->GetFrame()
                                ->GetDocument()
                                ->getElementsByTagName(AtomicString("iframe"))
                                ->item(0));
  ASSERT_TRUE(iframe_element);

  // Access the iframe's document and set some content
  auto* iframe_doc = iframe_element->contentDocument();
  ASSERT_TRUE(iframe_doc);

  iframe_doc->body()->setInnerHTML("<body>inside iframe</body>");

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& iframe = *root.children_nodes[0];
  const auto& iframe_attributes = *iframe.content_attributes;

  EXPECT_EQ(iframe_attributes.attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);

  const auto& iframe_root = iframe.children_nodes[0];
  const auto& iframe_root_attributes = *iframe_root->content_attributes;

  ASSERT_EQ(iframe_root_attributes.text_info.size(), 1u);
  EXPECT_EQ(iframe_root_attributes.text_info[0]->text_content, "inside iframe");
}

TEST_F(AIPageContentAgentTest, NoLayoutElement) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <div style='display: none;'>Hidden Content</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_TRUE(root.children_nodes.empty());
  EXPECT_TRUE(root.content_attributes->text_info.empty());
}

TEST_F(AIPageContentAgentTest, VisibilityHidden) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <div style='visibility: hidden;'>Hidden Content</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_TRUE(root.children_nodes.empty());
  EXPECT_TRUE(root.content_attributes->text_info.empty());
}

TEST_F(AIPageContentAgentTest, TextSize) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <h1>Extra large text</h1>"
      "  <h2>Large text</h2>"
      "  <p>Regular text</p>"
      "  <h6>Small text</h6>"
      "  <p style='font-size: 0.25em;'>Extra small text</p>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 5u);

  const auto& xl_text = *root.children_nodes[0]->content_attributes;
  ASSERT_EQ(xl_text.text_info.size(), 1u);
  EXPECT_EQ(xl_text.text_info[0]->text_style->text_size,
            mojom::blink::AIPageContentTextSize::kXL);

  const auto& l_text = *root.children_nodes[1]->content_attributes;
  ASSERT_EQ(l_text.text_info.size(), 1u);
  EXPECT_EQ(l_text.text_info[0]->text_style->text_size,
            mojom::blink::AIPageContentTextSize::kL);

  const auto& m_text = *root.children_nodes[2]->content_attributes;
  ASSERT_EQ(m_text.text_info.size(), 1u);
  EXPECT_EQ(m_text.text_info[0]->text_style->text_size,
            mojom::blink::AIPageContentTextSize::kM);

  const auto& s_text = *root.children_nodes[3]->content_attributes;
  ASSERT_EQ(s_text.text_info.size(), 1u);
  EXPECT_EQ(s_text.text_info[0]->text_style->text_size,
            mojom::blink::AIPageContentTextSize::kS);

  const auto& xs_text = *root.children_nodes[4]->content_attributes;
  ASSERT_EQ(xs_text.text_info.size(), 1u);
  EXPECT_EQ(xs_text.text_info[0]->text_style->text_size,
            mojom::blink::AIPageContentTextSize::kXS);
}

TEST_F(AIPageContentAgentTest, TextEmphasis) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "<p>Regular text"
      "<b>Bolded text</b>"
      "<i>Italicized text</i>"
      "<u>Underlined text</u>"
      "<sub>Subscript text</sub>"
      "<sup>Superscript text</sup>"
      "<em>Emphasized text</em>"
      "<strong>Strong text</strong>"
      "</p>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 1u);
  const auto& text = *root.children_nodes[0]->content_attributes;
  ASSERT_EQ(text.text_info.size(), 8u);

  EXPECT_EQ(text.text_info[0]->text_content, "Regular text");
  EXPECT_FALSE(text.text_info[0]->text_style->has_emphasis);

  EXPECT_EQ(text.text_info[1]->text_content, "Bolded text");
  EXPECT_TRUE(text.text_info[1]->text_style->has_emphasis);

  EXPECT_EQ(text.text_info[2]->text_content, "Italicized text");
  EXPECT_TRUE(text.text_info[2]->text_style->has_emphasis);

  EXPECT_EQ(text.text_info[3]->text_content, "Underlined text");
  EXPECT_TRUE(text.text_info[3]->text_style->has_emphasis);

  EXPECT_EQ(text.text_info[4]->text_content, "Subscript text");
  EXPECT_TRUE(text.text_info[4]->text_style->has_emphasis);

  EXPECT_EQ(text.text_info[5]->text_content, "Superscript text");
  EXPECT_TRUE(text.text_info[5]->text_style->has_emphasis);

  EXPECT_EQ(text.text_info[6]->text_content, "Emphasized text");
  EXPECT_TRUE(text.text_info[6]->text_style->has_emphasis);

  EXPECT_EQ(text.text_info[7]->text_content, "Strong text");
  EXPECT_TRUE(text.text_info[7]->text_style->has_emphasis);
}

}  // namespace
}  // namespace blink
