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

  void CheckListItemWithText(const mojom::blink::AIPageContentNode& node,
                             const String& expected_text) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kListItem);
    EXPECT_EQ(node.children_nodes.size(), 1u);
    CheckTextNode(*node.children_nodes[0], expected_text);
  }

  void CheckTextNode(const mojom::blink::AIPageContentNode& node,
                     const String& expected_text) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kText);
    ASSERT_TRUE(attributes.text_info);
    EXPECT_EQ(attributes.text_info->text_content, expected_text);
  }

  void CheckTextSize(const mojom::blink::AIPageContentNode& node,
                     mojom::blink::AIPageContentTextSize expected_text_size) {
    const auto& attributes = *node.content_attributes;
    ASSERT_TRUE(attributes.text_info);
    EXPECT_EQ(attributes.text_info->text_style->text_size, expected_text_size);
  }

  void CheckTextEmphasis(const mojom::blink::AIPageContentNode& node,
                         bool expected_has_emphasis) {
    const auto& attributes = *node.content_attributes;
    ASSERT_TRUE(attributes.text_info);
    EXPECT_EQ(attributes.text_info->text_style->has_emphasis,
              expected_has_emphasis);
  }

  void CheckImageNode(const mojom::blink::AIPageContentNode& node,
                      const String& expected_caption) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kImage);
    ASSERT_TRUE(attributes.image_info);
    EXPECT_EQ(attributes.image_info->image_caption, expected_caption);
  }

  void CheckAnchorNode(
      const mojom::blink::AIPageContentNode& node,
      const blink::KURL& expected_url,
      const Vector<mojom::blink::AIPageContentAnchorRel>& expected_rels) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kAnchor);
    ASSERT_TRUE(attributes.anchor_data);
    EXPECT_EQ(attributes.anchor_data->url, expected_url);
    ASSERT_EQ(attributes.anchor_data->rel.size(), expected_rels.size());
    for (size_t i = 0; i < expected_rels.size(); ++i) {
      EXPECT_EQ(attributes.anchor_data->rel[i], expected_rels[i]);
    }
  }

  void CheckTableNode(
      const mojom::blink::AIPageContentNode& node,
      std::optional<String> expected_table_name = std::nullopt) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kTable);
    ASSERT_TRUE(attributes.table_data);
    if (expected_table_name) {
      EXPECT_EQ(attributes.table_data->table_name, *expected_table_name);
    }
  }

  void CheckTableCellNode(const mojom::blink::AIPageContentNode& node) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kTableCell);
  }

  void CheckTableRowNode(
      const mojom::blink::AIPageContentNode& node,
      const mojom::blink::AIPageContentTableRowType& expected_row_type) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kTableRow);
    ASSERT_TRUE(attributes.table_row_data);
    EXPECT_EQ(attributes.table_row_data->row_type, expected_row_type);
  }

  void CheckContainerNode(const mojom::blink::AIPageContentNode& node) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kContainer);
  }

  void CheckHeadingNode(const mojom::blink::AIPageContentNode& node) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kHeading);
  }

  void CheckParagraphNode(const mojom::blink::AIPageContentNode& node) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kParagraph);
  }

  void CheckUnorderedListNode(const mojom::blink::AIPageContentNode& node) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kUnorderedList);
  }

  void CheckOrderedListNode(const mojom::blink::AIPageContentNode& node) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kOrderedList);
  }

  void CheckAnnotatedRole(
      const mojom::blink::AIPageContentNode& node,
      const mojom::blink::AIPageContentAnnotatedRole& expected_role) {
    const auto& attributes = *node.content_attributes;
    ASSERT_EQ(attributes.annotated_roles.size(), 1u);
    EXPECT_EQ(attributes.annotated_roles[0], expected_role);
  }

  void CheckGeometry(const mojom::blink::AIPageContentNode& node,
                     const gfx::Rect& expected_outer_bounding_box,
                     const gfx::Rect& expected_visible_bounding_box) {
    const auto& geometry = *node.content_attributes->geometry;
    EXPECT_EQ(geometry.outer_bounding_box, expected_outer_bounding_box);
    EXPECT_EQ(geometry.visible_bounding_box, expected_visible_bounding_box);
  }

  const mojom::blink::AIPageContentNode& GetSingleTableCell(
      const mojom::blink::AIPageContentNode& table) {
    CheckTableNode(table);
    EXPECT_EQ(table.children_nodes.size(), 1u);

    const auto& table_row = *table.children_nodes[0];
    CheckTableRowNode(table_row,
                      mojom::blink::AIPageContentTableRowType::kBody);
    EXPECT_EQ(table_row.children_nodes.size(), 1u);

    const auto& table_cell = *table_row.children_nodes[0];
    CheckTableCellNode(table_cell);
    return table_cell;
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
      "      position: absolute;"
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
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& attributes = *root.content_attributes;
  EXPECT_EQ(attributes.dom_node_ids.size(), 1u);
  EXPECT_TRUE(attributes.common_ancestor_dom_node_id.has_value());

  EXPECT_EQ(attributes.attribute_type,
            mojom::blink::AIPageContentAttributeType::kRoot);

  CheckGeometry(root, gfx::Rect(kWindowSize), gfx::Rect(kWindowSize));

  const auto& text_node = *root.children_nodes[0];
  CheckTextNode(text_node, "text");

  const auto& text_attributes = *text_node.content_attributes;
  ASSERT_TRUE(text_attributes.geometry);
  EXPECT_EQ(text_attributes.geometry->outer_bounding_box.x(), -20);
  EXPECT_EQ(text_attributes.geometry->outer_bounding_box.y(), -10);
}

TEST_F(AIPageContentAgentTest, Image) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    img {"
      "      position: absolute;"
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
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& attributes = *root.content_attributes;
  EXPECT_EQ(attributes.dom_node_ids.size(), 1u);

  auto& image_node = *root.children_nodes[0];
  CheckImageNode(image_node, "missing");
  CheckGeometry(image_node, gfx::Rect(-20, -10, 30, 40),
                gfx::Rect(0, 0, 10, 30));
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

  const auto& heading1 = *root.children_nodes[0];
  CheckHeadingNode(heading1);
  ASSERT_EQ(heading1.children_nodes.size(), 1u);
  const auto& heading1_text_node = *heading1.children_nodes[0];
  CheckTextNode(heading1_text_node, "Heading 1");

  const auto& heading2 = *root.children_nodes[1];
  CheckHeadingNode(heading2);
  ASSERT_EQ(heading2.children_nodes.size(), 1u);
  const auto& heading2_text_node = *heading2.children_nodes[0];
  CheckTextNode(heading2_text_node, "Heading 2");

  const auto& heading3 = *root.children_nodes[2];
  CheckHeadingNode(heading3);
  ASSERT_EQ(heading3.children_nodes.size(), 1u);
  const auto& heading3_text_node = *heading3.children_nodes[0];
  CheckTextNode(heading3_text_node, "Heading 3");
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

  const auto& paragraph = *root.children_nodes[0];
  CheckParagraphNode(paragraph);
  CheckGeometry(paragraph, gfx::Rect(-20, -10, 200, 40),
                gfx::Rect(0, 0, 180, 30));

  ASSERT_EQ(paragraph.children_nodes.size(), 1u);
  const auto& paragraph_text_node = *paragraph.children_nodes[0];
  CheckTextNode(paragraph_text_node, "text inside paragraph");
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

  const auto& ul = *root.children_nodes[0];
  CheckUnorderedListNode(ul);
  ASSERT_EQ(ul.children_nodes.size(), 2u);
  CheckListItemWithText(*ul.children_nodes[0], "Item 1");
  CheckListItemWithText(*ul.children_nodes[1], "Item 2");

  const auto& ol = *root.children_nodes[1];
  CheckOrderedListNode(ol);
  ASSERT_EQ(ol.children_nodes.size(), 2u);
  CheckListItemWithText(*ol.children_nodes[0], "Step 1");
  CheckListItemWithText(*ol.children_nodes[1], "Step 2");

  const auto& dl = *root.children_nodes[2];
  CheckUnorderedListNode(dl);
  ASSERT_EQ(dl.children_nodes.size(), 4u);
  CheckListItemWithText(*dl.children_nodes[0], "Detail 1 title");
  CheckListItemWithText(*dl.children_nodes[1], "Detail 1 description");
  CheckListItemWithText(*dl.children_nodes[2], "Detail 2 title");
  CheckListItemWithText(*dl.children_nodes[3], "Detail 2 description");
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

  const auto& iframe_root = *iframe.children_nodes[0];
  CheckTextNode(*iframe_root.children_nodes[0], "inside iframe");
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

  const auto& xl_text = *root.children_nodes[0];
  CheckHeadingNode(xl_text);
  CheckTextNode(*xl_text.children_nodes[0], "Extra large text");
  CheckTextSize(*xl_text.children_nodes[0],
                mojom::blink::AIPageContentTextSize::kXL);

  const auto& l_text = *root.children_nodes[1];
  CheckHeadingNode(l_text);
  CheckTextNode(*l_text.children_nodes[0], "Large text");
  CheckTextSize(*l_text.children_nodes[0],
                mojom::blink::AIPageContentTextSize::kL);

  const auto& m_text = *root.children_nodes[2];
  CheckParagraphNode(m_text);
  CheckTextNode(*m_text.children_nodes[0], "Regular text");
  CheckTextSize(*m_text.children_nodes[0],
                mojom::blink::AIPageContentTextSize::kM);

  const auto& s_text = *root.children_nodes[3];
  CheckHeadingNode(s_text);
  CheckTextNode(*s_text.children_nodes[0], "Small text");
  CheckTextSize(*s_text.children_nodes[0],
                mojom::blink::AIPageContentTextSize::kS);

  const auto& xs_text = *root.children_nodes[4];
  CheckParagraphNode(xs_text);
  CheckTextNode(*xs_text.children_nodes[0], "Extra small text");
  CheckTextSize(*xs_text.children_nodes[0],
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

  const auto& paragraph = *root.children_nodes[0];
  CheckParagraphNode(paragraph);
  ASSERT_EQ(paragraph.children_nodes.size(), 8u);

  const auto& regular_text = *paragraph.children_nodes[0];
  CheckTextNode(regular_text, "Regular text");
  CheckTextEmphasis(regular_text, false);

  const auto& bolded_text = *paragraph.children_nodes[1];
  CheckTextNode(bolded_text, "Bolded text");
  CheckTextEmphasis(bolded_text, true);

  const auto& italicized_text = *paragraph.children_nodes[2];
  CheckTextNode(italicized_text, "Italicized text");
  CheckTextEmphasis(italicized_text, true);

  const auto& underlined_text = *paragraph.children_nodes[3];
  CheckTextNode(underlined_text, "Underlined text");
  CheckTextEmphasis(underlined_text, true);

  const auto& subscript_text = *paragraph.children_nodes[4];
  CheckTextNode(subscript_text, "Subscript text");
  CheckTextEmphasis(subscript_text, true);

  const auto& superscript_text = *paragraph.children_nodes[5];
  CheckTextNode(superscript_text, "Superscript text");
  CheckTextEmphasis(superscript_text, true);

  const auto& emphasized_text = *paragraph.children_nodes[6];
  CheckTextNode(emphasized_text, "Emphasized text");
  CheckTextEmphasis(emphasized_text, true);

  const auto& strong_text = *paragraph.children_nodes[7];
  CheckTextNode(strong_text, "Strong text");
  CheckTextEmphasis(strong_text, true);
}

TEST_F(AIPageContentAgentTest, Table) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <table>"
      "    <caption>Table caption</caption>"
      "    <thead>"
      "      <th colspan='2'>Header</th>"
      "    </thead>"
      "    <tr>"
      "      <td>Row 1 Column 1</td>"
      "      <td>Row 1 Column 2</td>"
      "    </tr>"
      "    <tr>"
      "      <td>Row 2 Column 1</td>"
      "      <td>Row 2 Column 2</td>"
      "    </tr>"
      "    <tfoot>"
      "      <td>Footer 1</td>"
      "      <td>Footer 2</td>"
      "    </tfoot>"
      "  </table>"
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

  const auto& table = *root.children_nodes[0];
  CheckTableNode(table, "Table caption");
  ASSERT_EQ(table.children_nodes.size(), 4u);

  const auto& header1 = *table.children_nodes[0];
  CheckTableRowNode(header1, mojom::blink::AIPageContentTableRowType::kHeader);
  ASSERT_EQ(header1.children_nodes.size(), 1u);

  const auto& header1_cell1 = *header1.children_nodes[0];
  CheckTableCellNode(header1_cell1);
  CheckTextNode(*header1_cell1.children_nodes[0], "Header");

  const auto& row1 = *table.children_nodes[1];
  CheckTableRowNode(row1, mojom::blink::AIPageContentTableRowType::kBody);
  ASSERT_EQ(row1.children_nodes.size(), 2u);

  const auto& row1_cell1 = *row1.children_nodes[0];
  CheckTableCellNode(row1_cell1);
  CheckTextNode(*row1_cell1.children_nodes[0], "Row 1 Column 1");

  const auto& row1_cell2 = *row1.children_nodes[1];
  CheckTableCellNode(row1_cell2);
  CheckTextNode(*row1_cell2.children_nodes[0], "Row 1 Column 2");

  const auto& row2 = *table.children_nodes[2];
  CheckTableRowNode(row2, mojom::blink::AIPageContentTableRowType::kBody);
  ASSERT_EQ(row2.children_nodes.size(), 2u);

  const auto& row2_cell1 = *row2.children_nodes[0];
  CheckTableCellNode(row2_cell1);
  CheckTextNode(*row2_cell1.children_nodes[0], "Row 2 Column 1");

  const auto& row2_cell2 = *row2.children_nodes[1];
  CheckTableCellNode(row2_cell2);
  CheckTextNode(*row2_cell2.children_nodes[0], "Row 2 Column 2");

  const auto& footer = *table.children_nodes[3];
  CheckTableRowNode(footer, mojom::blink::AIPageContentTableRowType::kFooter);
  ASSERT_EQ(footer.children_nodes.size(), 2u);

  const auto& footer_cell1 = *footer.children_nodes[0];
  CheckTableCellNode(footer_cell1);
  CheckTextNode(*footer_cell1.children_nodes[0], "Footer 1");

  const auto& footer_cell2 = *footer.children_nodes[1];
  CheckTableCellNode(footer_cell2);
  CheckTextNode(*footer_cell2.children_nodes[0], "Footer 2");
}

TEST_F(AIPageContentAgentTest, TableMadeWithCss) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "    <style>"
      "        .table {"
      "            display: table;"
      "            border-collapse: collapse;"
      "            width: 100%;"
      "        }"
      "        .row {"
      "            display: table-row;"
      "        }"
      "        .cell {"
      "            display: table-cell;"
      "            border: 1px solid #000;"
      "            padding: 8px;"
      "            text-align: center;"
      "        }"
      "        .header {"
      "            background-color: #f4f4f4;"
      "            font-weight: bold;"
      "        }"
      "    </style>"
      "    <div class='table'>"
      //       Header Rows
      "        <div class='row header'>"
      "            <div class='cell' colspan='2'>Personal Info</div>"
      "            <div class='cell' colspan='2'>Contact Info</div>"
      "        </div>"
      "        <div class='row header'>"
      "            <div class='cell'>Name</div>"
      "            <div class='cell'>Age</div>"
      "            <div class='cell'>Email</div>"
      "            <div class='cell'>Phone</div>"
      "        </div>"
      //       Body Rows
      "        <div class='row'>"
      "            <div class='cell'>John Doe</div>"
      "            <div class='cell'>30</div>"
      "            <div class='cell'>john.doe@example.com</div>"
      "            <div class='cell'>123-456-7890</div>"
      "        </div>"
      "        <div class='row'>"
      "            <div class='cell'>Jane Smith</div>"
      "            <div class='cell'>28</div>"
      "            <div class='cell'>jane.smith@example.com</div>"
      "            <div class='cell'>987-654-3210</div>"
      "        </div>"
      "    </div>"
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

  const auto& table = *root.children_nodes[0];
  CheckTableNode(table);
  ASSERT_EQ(table.children_nodes.size(), 4u);

  const auto& row1 = *table.children_nodes[0];
  CheckTableRowNode(row1, mojom::blink::AIPageContentTableRowType::kBody);
  ASSERT_EQ(row1.children_nodes.size(), 2u);

  const auto& row1_cell1 = *row1.children_nodes[0];
  CheckTableCellNode(row1_cell1);
  CheckTextNode(*row1_cell1.children_nodes[0], "Personal Info");

  const auto& row1_cell2 = *row1.children_nodes[1];
  CheckTableCellNode(row1_cell2);
  CheckTextNode(*row1_cell2.children_nodes[0], "Contact Info");

  const auto& row2 = *table.children_nodes[1];
  CheckTableRowNode(row2, mojom::blink::AIPageContentTableRowType::kBody);
  ASSERT_EQ(row2.children_nodes.size(), 4u);

  const auto& row2_cell1 = *row2.children_nodes[0];
  CheckTableCellNode(row2_cell1);
  CheckTextNode(*row2_cell1.children_nodes[0], "Name");

  const auto& row2_cell2 = *row2.children_nodes[1];
  CheckTableCellNode(row2_cell2);
  CheckTextNode(*row2_cell2.children_nodes[0], "Age");

  const auto& row2_cell3 = *row2.children_nodes[2];
  CheckTableCellNode(row2_cell3);
  CheckTextNode(*row2_cell3.children_nodes[0], "Email");

  const auto& row2_cell4 = *row2.children_nodes[3];
  CheckTableCellNode(row2_cell4);
  CheckTextNode(*row2_cell4.children_nodes[0], "Phone");

  const auto& row3 = *table.children_nodes[2];
  CheckTableRowNode(row3, mojom::blink::AIPageContentTableRowType::kBody);
  ASSERT_EQ(row3.children_nodes.size(), 4u);

  const auto& row3_cell1 = *row3.children_nodes[0];
  CheckTableCellNode(row3_cell1);
  CheckTextNode(*row3_cell1.children_nodes[0], "John Doe");

  const auto& row3_cell2 = *row3.children_nodes[1];
  CheckTableCellNode(row3_cell2);
  CheckTextNode(*row3_cell2.children_nodes[0], "30");

  const auto& row3_cell3 = *row3.children_nodes[2];
  CheckTableCellNode(row3_cell3);
  CheckTextNode(*row3_cell3.children_nodes[0], "john.doe@example.com");

  const auto& row3_cell4 = *row3.children_nodes[3];
  CheckTableCellNode(row3_cell4);
  CheckTextNode(*row3_cell4.children_nodes[0], "123-456-7890");

  const auto& row4 = *table.children_nodes[3];
  CheckTableRowNode(row4, mojom::blink::AIPageContentTableRowType::kBody);
  ASSERT_EQ(row4.children_nodes.size(), 4u);

  const auto& row4_cell1 = *row4.children_nodes[0];
  CheckTableCellNode(row4_cell1);
  CheckTextNode(*row4_cell1.children_nodes[0], "Jane Smith");

  const auto& row4_cell2 = *row4.children_nodes[1];
  CheckTableCellNode(row4_cell2);
  CheckTextNode(*row4_cell2.children_nodes[0], "28");

  const auto& row4_cell3 = *row4.children_nodes[2];
  CheckTableCellNode(row4_cell3);
  CheckTextNode(*row4_cell3.children_nodes[0], "jane.smith@example.com");

  const auto& row4_cell4 = *row4.children_nodes[3];
  CheckTableCellNode(row4_cell4);
  CheckTextNode(*row4_cell4.children_nodes[0], "987-654-3210");
}

TEST_F(AIPageContentAgentTest, LandmarkSections) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <header>Header</header>"
      "  <nav>Navigation</nav>"
      "  <search>Search</search>"
      "  <main>Main content</main>"
      "  <article>Article</article>"
      "  <section>Section</section>"
      "  <aside>Aside</aside>"
      "  <footer>Footer</footer>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 8u);

  const auto& header = *root.children_nodes[0];
  CheckContainerNode(header);
  CheckAnnotatedRole(header, mojom::blink::AIPageContentAnnotatedRole::kHeader);
  CheckTextNode(*header.children_nodes[0], "Header");

  const auto& nav = *root.children_nodes[1];
  CheckContainerNode(nav);
  CheckAnnotatedRole(nav, mojom::blink::AIPageContentAnnotatedRole::kNav);
  CheckTextNode(*nav.children_nodes[0], "Navigation");

  const auto& search = *root.children_nodes[2];
  CheckContainerNode(search);
  CheckAnnotatedRole(search, mojom::blink::AIPageContentAnnotatedRole::kSearch);
  CheckTextNode(*search.children_nodes[0], "Search");

  const auto& main = *root.children_nodes[3];
  CheckContainerNode(main);
  CheckAnnotatedRole(main, mojom::blink::AIPageContentAnnotatedRole::kMain);
  CheckTextNode(*main.children_nodes[0], "Main content");

  const auto& article = *root.children_nodes[4];
  CheckContainerNode(article);
  CheckAnnotatedRole(article,
                     mojom::blink::AIPageContentAnnotatedRole::kArticle);
  CheckTextNode(*article.children_nodes[0], "Article");

  const auto& section = *root.children_nodes[5];
  CheckContainerNode(section);
  CheckAnnotatedRole(section,
                     mojom::blink::AIPageContentAnnotatedRole::kSection);
  CheckTextNode(*section.children_nodes[0], "Section");

  const auto& aside = *root.children_nodes[6];
  CheckContainerNode(aside);
  CheckAnnotatedRole(aside, mojom::blink::AIPageContentAnnotatedRole::kAside);
  CheckTextNode(*aside.children_nodes[0], "Aside");

  const auto& footer = *root.children_nodes[7];
  CheckContainerNode(footer);
  CheckAnnotatedRole(footer, mojom::blink::AIPageContentAnnotatedRole::kFooter);
  CheckTextNode(*footer.children_nodes[0], "Footer");
}

TEST_F(AIPageContentAgentTest, LandmarkSectionsWithAriaRoles) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <div role='banner'>Header</div>"
      "  <div role='navigation'>Navigation</div>"
      "  <div role='search'>Search</div>"
      "  <div role='main'>Main content</div>"
      "  <div role='article'>Article</div>"
      "  <div role='region'>Section</div>"
      "  <div role='complementary'>Aside</div>"
      "  <div role='contentinfo'>Footer</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 8u);

  const auto& header = *root.children_nodes[0];
  CheckContainerNode(header);
  CheckAnnotatedRole(header, mojom::blink::AIPageContentAnnotatedRole::kHeader);
  CheckTextNode(*header.children_nodes[0], "Header");

  const auto& nav = *root.children_nodes[1];
  CheckContainerNode(nav);
  CheckAnnotatedRole(nav, mojom::blink::AIPageContentAnnotatedRole::kNav);
  CheckTextNode(*nav.children_nodes[0], "Navigation");

  const auto& search = *root.children_nodes[2];
  CheckContainerNode(search);
  CheckAnnotatedRole(search, mojom::blink::AIPageContentAnnotatedRole::kSearch);
  CheckTextNode(*search.children_nodes[0], "Search");

  const auto& main = *root.children_nodes[3];
  CheckContainerNode(main);
  CheckAnnotatedRole(main, mojom::blink::AIPageContentAnnotatedRole::kMain);
  CheckTextNode(*main.children_nodes[0], "Main content");

  const auto& article = *root.children_nodes[4];
  CheckContainerNode(article);
  CheckAnnotatedRole(article,
                     mojom::blink::AIPageContentAnnotatedRole::kArticle);
  CheckTextNode(*article.children_nodes[0], "Article");

  const auto& section = *root.children_nodes[5];
  CheckContainerNode(section);
  CheckAnnotatedRole(section,
                     mojom::blink::AIPageContentAnnotatedRole::kSection);
  CheckTextNode(*section.children_nodes[0], "Section");

  const auto& aside = *root.children_nodes[6];
  CheckContainerNode(aside);
  CheckAnnotatedRole(aside, mojom::blink::AIPageContentAnnotatedRole::kAside);
  CheckTextNode(*aside.children_nodes[0], "Aside");

  const auto& footer = *root.children_nodes[7];
  CheckContainerNode(footer);
  CheckAnnotatedRole(footer, mojom::blink::AIPageContentAnnotatedRole::kFooter);
  CheckTextNode(*footer.children_nodes[0], "Footer");
}

TEST_F(AIPageContentAgentTest, FixedPosition) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "     <body>"
      "       <style>"
      "       .fixed {"
      "         position: fixed;"
      "         top: 50px;"
      "         left: 50px;"
      "         width: 200px;"
      "       }"
      "       .sticky {"
      "         position: sticky;"
      "         top: 50px;"
      "         left: 3000px;"
      "         width: 200px;"
      "       }"
      "       .normal {"
      "         width: 250px;"
      "         height: 80px;"
      "         margin-top: 20px;"
      "       }"
      "       </style>"
      "       <div class='fixed'>This element stays in place when the page is "
      "scrolled.</div>"
      "       <div class='sticky'>This element stays in place when the page is "
      "scrolled.</div>"
      "       <div class='normal'>This element flows naturally with the "
      "document.</div>"
      "     </body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 3u);

  EXPECT_FALSE(root.content_attributes->geometry->is_fixed_or_sticky_position);

  const auto& fixed_element = *root.children_nodes[0];
  CheckContainerNode(fixed_element);
  EXPECT_TRUE(
      fixed_element.content_attributes->geometry->is_fixed_or_sticky_position);
  EXPECT_FALSE(fixed_element.content_attributes->geometry->scrolls_overflow_x);
  EXPECT_FALSE(fixed_element.content_attributes->geometry->scrolls_overflow_y);
  CheckTextNode(*fixed_element.children_nodes[0],
                "This element stays in place when the page is scrolled.");

  const auto& sticky_element = *root.children_nodes[1];
  CheckContainerNode(sticky_element);
  EXPECT_TRUE(
      sticky_element.content_attributes->geometry->is_fixed_or_sticky_position);
  EXPECT_FALSE(sticky_element.content_attributes->geometry->scrolls_overflow_x);
  EXPECT_FALSE(sticky_element.content_attributes->geometry->scrolls_overflow_y);
  CheckTextNode(*sticky_element.children_nodes[0],
                "This element stays in place when the page is scrolled.");

  const auto& normal_element = *root.children_nodes[2];
  EXPECT_FALSE(
      normal_element.content_attributes->geometry->is_fixed_or_sticky_position);
  EXPECT_FALSE(normal_element.content_attributes->geometry->scrolls_overflow_x);
  EXPECT_FALSE(normal_element.content_attributes->geometry->scrolls_overflow_y);
  CheckTextNode(normal_element,
                "This element flows naturally with the document.");
}

TEST_F(AIPageContentAgentTest, ScrollContainer) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "     <body>"
      "       <style>"
      "       .scrollable-x {"
      "         width: 100px;"
      "         height: 50px;"
      "         overflow-x: scroll;"
      "         overflow-y: clip;"
      "       }"
      "       .scrollable-y {"
      "         width: 300px;"
      "         height: 50px;"
      "         overflow-x: clip;"
      "         overflow-y: scroll;"
      "       }"
      "       .auto-scroll-x {"
      "         width: 100px;"
      "         height: 50px;"
      "         overflow-x: auto;"
      "         overflow-y: clip;"
      "       }"
      "       .auto-scroll-y {"
      "         width: 300px;"
      "         height: 50px;"
      "         overflow-x: clip;"
      "         overflow-y: auto;"
      "       }"
      "       .normal {"
      "         width: 250px;"
      "         height: 80px;"
      "         margin-top: 20px;"
      "       }"
      "       </style>"
      "       <div "
      "class='scrollable-x'>"
      "ABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVW"
      "XYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRST"
      "UVWXYZ</div>"
      "       <div class='scrollable-y'>Some long text to make it scrollable. "
      "Some long text to make it scrollable. Some long text to make it "
      "scrollable. Some long text to make it scrollable.</div>"
      "       <div "
      "class='auto-scroll-x'>"
      "ABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVW"
      "XYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRST"
      "UVWXYZ</div>"
      "       <div class='auto-scroll-y'>Some long text to make it scrollable. "
      "Some long text to make it scrollable. Some long text to make it "
      "scrollable. Some long text to make it scrollable.</div>"
      "     </body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 4u);

  EXPECT_TRUE(root.content_attributes->geometry->scrolls_overflow_x);
  EXPECT_TRUE(root.content_attributes->geometry->scrolls_overflow_y);

  const auto& scrollable_x_element = *root.children_nodes[0];
  CheckContainerNode(scrollable_x_element);
  EXPECT_FALSE(scrollable_x_element.content_attributes->geometry
                   ->is_fixed_or_sticky_position);
  EXPECT_TRUE(
      scrollable_x_element.content_attributes->geometry->scrolls_overflow_x);
  EXPECT_FALSE(
      scrollable_x_element.content_attributes->geometry->scrolls_overflow_y);
  CheckTextNode(
      *scrollable_x_element.children_nodes[0],
      "ABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVW"
      "XYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRST"
      "UVWXYZ");

  const auto& scrollable_y_element = *root.children_nodes[1];
  CheckContainerNode(scrollable_y_element);
  EXPECT_FALSE(scrollable_y_element.content_attributes->geometry
                   ->is_fixed_or_sticky_position);
  EXPECT_FALSE(
      scrollable_y_element.content_attributes->geometry->scrolls_overflow_x);
  EXPECT_TRUE(
      scrollable_y_element.content_attributes->geometry->scrolls_overflow_y);
  CheckTextNode(*scrollable_y_element.children_nodes[0],
                "Some long text to make it scrollable. Some long text to make "
                "it scrollable. Some long text to make it scrollable. Some "
                "long text to make it scrollable.");

  const auto& auto_scroll_x_element = *root.children_nodes[2];
  CheckContainerNode(auto_scroll_x_element);
  EXPECT_FALSE(auto_scroll_x_element.content_attributes->geometry
                   ->is_fixed_or_sticky_position);
  EXPECT_TRUE(
      auto_scroll_x_element.content_attributes->geometry->scrolls_overflow_x);
  EXPECT_FALSE(
      auto_scroll_x_element.content_attributes->geometry->scrolls_overflow_y);
  CheckTextNode(
      *auto_scroll_x_element.children_nodes[0],
      "ABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVW"
      "XYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRST"
      "UVWXYZ");

  const auto& auto_scroll_y_element = *root.children_nodes[3];
  CheckContainerNode(auto_scroll_y_element);
  EXPECT_FALSE(auto_scroll_y_element.content_attributes->geometry
                   ->is_fixed_or_sticky_position);
  EXPECT_FALSE(
      auto_scroll_y_element.content_attributes->geometry->scrolls_overflow_x);
  EXPECT_TRUE(
      auto_scroll_y_element.content_attributes->geometry->scrolls_overflow_y);
  CheckTextNode(*auto_scroll_y_element.children_nodes[0],
                "Some long text to make it scrollable. Some long text to make "
                "it scrollable. Some long text to make it scrollable. Some "
                "long text to make it scrollable.");
}

TEST_F(AIPageContentAgentTest, Anchors) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <a href='https://www.google.com'>Google</a>"
      "  <a href='https://www.youtube.com' rel='noopener "
      "noreferrer'>YouTube</a>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 2u);

  const auto& link = *root.children_nodes[0];
  CheckAnchorNode(link, blink::KURL("https://www.google.com/"), {});
  const auto& link_text = *link.children_nodes[0];
  CheckTextNode(link_text, "Google");

  const auto& link_with_rel = *root.children_nodes[1];
  CheckAnchorNode(link_with_rel, blink::KURL("https://www.youtube.com/"),
                  {mojom::blink::AIPageContentAnchorRel::kRelationNoOpener,
                   mojom::blink::AIPageContentAnchorRel::kRelationNoReferrer});
  const auto& link_with_rel_text = *link_with_rel.children_nodes[0];
  CheckTextNode(link_with_rel_text, "YouTube");
}

TEST_F(AIPageContentAgentTest, TopLayerContainer) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <dialog id='welcomeDialog' style='position: absolute; overflow: "
      "visible;'>This is a dialog.</dialog>"
      "  <script>"
      "    const dialog = document.getElementById('welcomeDialog');"
      "    dialog.showModal();"
      "  </script>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  // Two nodes: the dialog and its backdrop.
  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 2u);

  const auto& backdrop = *root.children_nodes[0];
  CheckContainerNode(backdrop);
  EXPECT_TRUE(
      backdrop.content_attributes->geometry->is_fixed_or_sticky_position);

  const auto& dialog = *root.children_nodes[1];
  CheckContainerNode(dialog);

  ASSERT_EQ(dialog.children_nodes.size(), 1u);
  const auto& dialog_text = *dialog.children_nodes[0];
  CheckTextNode(dialog_text, "This is a dialog.");
}

TEST_F(AIPageContentAgentTest, TableWithAnonymousCells) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<html>"
      "  <style>"
      "    #target {"
      "      display: table;"
      "    }"
      ""
      "    #target::before {"
      "      content: 'BEFORE';"
      "      display: table;"
      "    }"
      "  </style>"
      "  <div id='target'></div>"
      "</html>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = agent->GetAIPageContentSync();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& outer_table = *root.children_nodes[0];
  const auto& outer_table_cell = GetSingleTableCell(outer_table);
  ASSERT_EQ(outer_table_cell.children_nodes.size(), 1u);

  const auto& inner_table = *outer_table_cell.children_nodes[0];
  const auto& inner_table_cell = GetSingleTableCell(inner_table);

  EXPECT_EQ(inner_table_cell.children_nodes.size(), 1u);
  CheckTextNode(*inner_table_cell.children_nodes[0], "BEFORE");
}

}  // namespace
}  // namespace blink
