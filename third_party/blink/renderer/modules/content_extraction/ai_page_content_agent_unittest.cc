// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
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
    helper_.InitializeWithSettings(&UpdateWebSettings);
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

  void CheckTextColor(const mojom::blink::AIPageContentNode& node,
                      RGBA32 expected_color) {
    const auto& attributes = *node.content_attributes;
    ASSERT_TRUE(attributes.text_info);
    EXPECT_EQ(attributes.text_info->text_style->color, expected_color);
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

  void CheckTableNode(const mojom::blink::AIPageContentNode& node,
                      String expected_table_name = String()) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kTable);
    ASSERT_TRUE(attributes.table_data);
    if (!expected_table_name.IsNull()) {
      EXPECT_EQ(attributes.table_data->table_name, expected_table_name);
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

  void CheckAnnotatedRoles(
      const mojom::blink::AIPageContentNode& node,
      const std::vector<mojom::blink::AIPageContentAnnotatedRole>&
          expected_roles) {
    const auto& attributes = *node.content_attributes;
    ASSERT_EQ(attributes.annotated_roles.size(), expected_roles.size());
    EXPECT_THAT(attributes.annotated_roles,
                testing::UnorderedElementsAreArray(expected_roles));
  }

  void CheckGeometry(const mojom::blink::AIPageContentNode& node,
                     const gfx::Rect& expected_outer_bounding_box,
                     const gfx::Rect& expected_visible_bounding_box) {
    const auto& geometry = *node.content_attributes->geometry;
    EXPECT_EQ(geometry.outer_bounding_box, expected_outer_bounding_box);
    EXPECT_EQ(geometry.visible_bounding_box, expected_visible_bounding_box);
  }

  void CheckFormControlNode(
      const mojom::blink::AIPageContentNode& node,
      const mojom::blink::FormControlType& expected_form_control_type) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kFormControl);
    EXPECT_EQ(attributes.form_control_data->form_control_type,
              expected_form_control_type);
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

  void CheckIframeNode(const mojom::blink::AIPageContentNode& node) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kIframe);
  }

  void CheckRootNode(const mojom::blink::AIPageContentNode& node) {
    const auto& attributes = *node.content_attributes;
    EXPECT_EQ(attributes.attribute_type,
              mojom::blink::AIPageContentAttributeType::kRoot);
  }

  void CheckAlmostEquals(const gfx::Point& actual, const gfx::Point& expected) {
    // Allow 1px difference for rounding.
    const int kTolerance = 1;
    EXPECT_LE(abs(actual.x() - expected.x()), kTolerance)
        << "actual : " << actual.ToString()
        << ", expected: " << expected.ToString();
    EXPECT_LE(abs(actual.y() - expected.y()), kTolerance)
        << "actual : " << actual.ToString()
        << ", expected: " << expected.ToString();
  }

  mojom::blink::AIPageContentPtr GetAIPageContent(
      std::optional<mojom::blink::AIPageContentOptions> options =
          std::nullopt) {
    auto* agent = AIPageContentAgent::GetOrCreateForTesting(
        *helper_.LocalMainFrame()->GetFrame()->GetDocument());
    EXPECT_TRUE(agent);

    return agent->GetAIPageContentInternal(options ? *options
                                                   : default_options_);
  }

  void FireMouseMoveEvent(const gfx::PointF& point) {
    EventHandler& event_handler =
        helper_.LocalMainFrame()->GetFrame()->GetEventHandler();
    WebMouseEvent event(WebInputEvent::Type::kMouseMove, point, point,
                        WebPointerProperties::Button::kLeft, 0,
                        WebInputEvent::kLeftButtonDown,
                        WebInputEvent::GetStaticTimeStampForTests());
    event.SetFrameScale(1);
    event_handler.HandleMouseMoveEvent(event, Vector<WebMouseEvent>(),
                                       Vector<WebMouseEvent>());
  }

 protected:
  const mojom::blink::AIPageContentOptions default_options_;
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;

 private:
  static void UpdateWebSettings(WebSettings* settings) {
    settings->SetTextAreasAreResizable(true);
  }
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();

  mojom::blink::AIPageContentPtr output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::blink::AIPageContent>(
      content, output));
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

TEST_F(AIPageContentAgentTest, TextColor) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "<p>Regular text</p>"
      "<p style='color: red'>Red text</p>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 2u);

  const auto& paragraph = *root.children_nodes[0];
  CheckParagraphNode(paragraph);
  ASSERT_EQ(paragraph.children_nodes.size(), 1u);

  const auto& regular_text = *paragraph.children_nodes[0];
  CheckTextNode(regular_text, "Regular text");
  CheckTextColor(regular_text, Color(0, 0, 0).Rgb());

  const auto& red_paragraph = *root.children_nodes[1];
  CheckParagraphNode(paragraph);
  ASSERT_EQ(paragraph.children_nodes.size(), 1u);

  const auto& bolded_text = *red_paragraph.children_nodes[0];
  CheckTextNode(bolded_text, "Red text");
  CheckTextColor(bolded_text, Color(255, 0, 0).Rgb());
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 3u);

  EXPECT_FALSE(root.content_attributes->geometry->is_fixed_or_sticky_position);

  const auto& fixed_element = *root.children_nodes[0];
  CheckContainerNode(fixed_element);
  EXPECT_TRUE(
      fixed_element.content_attributes->geometry->is_fixed_or_sticky_position);
  EXPECT_FALSE(fixed_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_x);
  EXPECT_FALSE(fixed_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_y);
  CheckTextNode(*fixed_element.children_nodes[0],
                "This element stays in place when the page is scrolled.");

  const auto& sticky_element = *root.children_nodes[1];
  CheckContainerNode(sticky_element);
  EXPECT_TRUE(
      sticky_element.content_attributes->geometry->is_fixed_or_sticky_position);
  EXPECT_FALSE(sticky_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_x);
  EXPECT_FALSE(sticky_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_y);
  CheckTextNode(*sticky_element.children_nodes[0],
                "This element stays in place when the page is scrolled.");

  const auto& normal_element = *root.children_nodes[2];
  EXPECT_FALSE(
      normal_element.content_attributes->geometry->is_fixed_or_sticky_position);
  EXPECT_FALSE(normal_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_x);
  EXPECT_FALSE(normal_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_y);
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

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  ASSERT_EQ(root.children_nodes.size(), 4u);

  EXPECT_TRUE(
      root.content_attributes->node_interaction_info->scrolls_overflow_x);
  EXPECT_TRUE(
      root.content_attributes->node_interaction_info->scrolls_overflow_y);

  const auto& scrollable_x_element = *root.children_nodes[0];
  CheckContainerNode(scrollable_x_element);
  EXPECT_FALSE(scrollable_x_element.content_attributes->geometry
                   ->is_fixed_or_sticky_position);
  EXPECT_TRUE(scrollable_x_element.content_attributes->node_interaction_info
                  ->scrolls_overflow_x);
  EXPECT_FALSE(scrollable_x_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_y);
  CheckTextNode(
      *scrollable_x_element.children_nodes[0],
      "ABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVW"
      "XYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRST"
      "UVWXYZ");

  const auto& scrollable_y_element = *root.children_nodes[1];
  CheckContainerNode(scrollable_y_element);
  EXPECT_FALSE(scrollable_y_element.content_attributes->geometry
                   ->is_fixed_or_sticky_position);
  EXPECT_FALSE(scrollable_y_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_x);
  EXPECT_TRUE(scrollable_y_element.content_attributes->node_interaction_info
                  ->scrolls_overflow_y);
  CheckTextNode(*scrollable_y_element.children_nodes[0],
                "Some long text to make it scrollable. Some long text to make "
                "it scrollable. Some long text to make it scrollable. Some "
                "long text to make it scrollable.");

  const auto& auto_scroll_x_element = *root.children_nodes[2];
  CheckContainerNode(auto_scroll_x_element);
  EXPECT_FALSE(auto_scroll_x_element.content_attributes->geometry
                   ->is_fixed_or_sticky_position);
  EXPECT_TRUE(auto_scroll_x_element.content_attributes->node_interaction_info
                  ->scrolls_overflow_x);
  EXPECT_FALSE(auto_scroll_x_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_y);
  CheckTextNode(
      *auto_scroll_x_element.children_nodes[0],
      "ABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVW"
      "XYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRSTUVWXYZABCDEFGHIJKLMOPQRST"
      "UVWXYZ");

  const auto& auto_scroll_y_element = *root.children_nodes[3];
  CheckContainerNode(auto_scroll_y_element);
  EXPECT_FALSE(auto_scroll_y_element.content_attributes->geometry
                   ->is_fixed_or_sticky_position);
  EXPECT_FALSE(auto_scroll_y_element.content_attributes->node_interaction_info
                   ->scrolls_overflow_x);
  EXPECT_TRUE(auto_scroll_y_element.content_attributes->node_interaction_info
                  ->scrolls_overflow_y);
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

  auto content = GetAIPageContent();
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

TEST_F(AIPageContentAgentTest, ContentVisibilityHidden) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    div {"
      "      content-visibility: hidden"
      "    }"
      "  </style>"
      "  <div>text</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& hidden_container = *root.children_nodes[0];
  CheckContainerNode(hidden_container);
  CheckAnnotatedRole(hidden_container,
                     mojom::blink::AIPageContentAnnotatedRole::kContentHidden);
  EXPECT_TRUE(hidden_container.children_nodes.empty());
}

TEST_F(AIPageContentAgentTest, ContentVisibilityAuto) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    #foo {"
      "      position: relative;"
      "      top: 8000px;"
      "      content-visibility: auto"
      "    }"
      "  </style>"
      "  <div id=foo><div>far text</div></div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& text_node = *root.children_nodes[0];
  CheckTextNode(text_node, "far text");

  const auto& attributes = *text_node.content_attributes;
  EXPECT_EQ(attributes.dom_node_ids.size(), 1u);
  EXPECT_TRUE(attributes.common_ancestor_dom_node_id.has_value());

  EXPECT_TRUE(attributes.geometry);
  EXPECT_FALSE(attributes.geometry->outer_bounding_box.IsEmpty());
  EXPECT_TRUE(attributes.geometry->visible_bounding_box.IsEmpty());
}

TEST_F(AIPageContentAgentTest, HiddenUntilFound) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    body {"
      "      margin: 0; font-size: 100px;"
      "    }"
      "  </style>"
      "  <header hidden=until-found>hidden text</header><div>visible text</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 2u);

  const auto& hidden_container = *root.children_nodes[0];
  CheckContainerNode(hidden_container);
  CheckAnnotatedRoles(
      hidden_container,
      {mojom::blink::AIPageContentAnnotatedRole::kHeader,
       mojom::blink::AIPageContentAnnotatedRole::kContentHidden});
  EXPECT_EQ(hidden_container.children_nodes.size(), 1u);

  // The hidden container continues to have an empty layout size even when
  // display locks are forced.
  ASSERT_TRUE(hidden_container.content_attributes->geometry);
  const auto& hidden_container_geometry =
      *hidden_container.content_attributes->geometry;
  EXPECT_TRUE(hidden_container_geometry.outer_bounding_box.IsEmpty());
  EXPECT_TRUE(hidden_container_geometry.visible_bounding_box.IsEmpty());

  const auto& hidden_text_node = *hidden_container.children_nodes[0];
  CheckTextNode(hidden_text_node, "hidden text");
  ASSERT_TRUE(hidden_text_node.content_attributes->geometry);
  const auto& hidden_text_geometry =
      *hidden_text_node.content_attributes->geometry;
  CheckAlmostEquals(hidden_text_geometry.outer_bounding_box.origin(),
                    gfx::Point(0, 0));
  EXPECT_FALSE(hidden_text_geometry.outer_bounding_box.IsEmpty());
  EXPECT_TRUE(hidden_text_geometry.visible_bounding_box.IsEmpty());

  const auto& visible_text_node = *root.children_nodes[1];
  CheckTextNode(visible_text_node, "visible text");
  EXPECT_TRUE(visible_text_node.content_attributes->geometry);
  const auto& visible_text_geometry =
      *visible_text_node.content_attributes->geometry;
  CheckAlmostEquals(visible_text_geometry.outer_bounding_box.origin(),
                    gfx::Point(0, 0));
  EXPECT_FALSE(visible_text_geometry.outer_bounding_box.IsEmpty());
  EXPECT_EQ(visible_text_geometry.outer_bounding_box,
            visible_text_geometry.visible_bounding_box);
}

TEST_F(AIPageContentAgentTest, HiddenUntilFoundInsideIframe) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    body {"
      "      margin: 0;"
      "      font-size: 100px;"
      "    }"
      "  </style>"
      "  <iframe srcdoc='<div hidden=until-found>hidden "
      "text</div>'></iframe>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& iframe_node = *root.children_nodes[0];
  EXPECT_EQ(iframe_node.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);
  EXPECT_EQ(iframe_node.children_nodes.size(), 1u);
  const auto& iframe_root = *iframe_node.children_nodes[0];

  EXPECT_EQ(iframe_root.children_nodes.size(), 1u);
  const auto& hidden_container = *iframe_root.children_nodes[0];
  CheckContainerNode(hidden_container);
  CheckAnnotatedRole(hidden_container,
                     mojom::blink::AIPageContentAnnotatedRole::kContentHidden);
  EXPECT_EQ(hidden_container.children_nodes.size(), 1u);

  const auto& hidden_text_node = *hidden_container.children_nodes[0];
  CheckTextNode(hidden_text_node, "hidden text");
  ASSERT_TRUE(hidden_text_node.content_attributes->geometry);
  const auto& hidden_text_geometry =
      *hidden_text_node.content_attributes->geometry;
  EXPECT_FALSE(hidden_text_geometry.outer_bounding_box.IsEmpty());
  EXPECT_TRUE(hidden_text_geometry.visible_bounding_box.IsEmpty());
}

TEST_F(AIPageContentAgentTest, HiddenUntilFoundOnIframe) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    body {"
      "      margin: 0;"
      "      font-size: 100px;"
      "    }"
      "  </style>"
      "  <iframe hidden=until-found srcdoc='<div>hidden "
      "text</div>'></iframe>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& iframe_node = *root.children_nodes[0];
  EXPECT_EQ(iframe_node.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);
  EXPECT_EQ(iframe_node.children_nodes.size(), 1u);
  const auto& iframe_root = *iframe_node.children_nodes[0];

  const auto& hidden_text_node = *iframe_root.children_nodes[0];
  CheckTextNode(hidden_text_node, "hidden text");
  ASSERT_TRUE(hidden_text_node.content_attributes->geometry);
  const auto& hidden_text_geometry =
      *hidden_text_node.content_attributes->geometry;
  EXPECT_FALSE(hidden_text_geometry.outer_bounding_box.IsEmpty());
  EXPECT_TRUE(hidden_text_geometry.visible_bounding_box.IsEmpty());
}

TEST_F(AIPageContentAgentTest, LineBreak) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "<div style=\"width: 100px; height:100px\">"
      "Lorem Ipsum is simply dummy text of the printing and "
      "typesetting industry.<br>Lorem Ipsum has been the "
      "industry's standard dummy text ever since the 1500s, "
      "when an unknown printer took a galley of type and "
      "scrambled it to make a type specimen book. It has "
      "survived not only five centuries, but also the leap "
      "into electronic typesetting, remaining essentially "
      "unchanged. It was popularised in the 1960s with the "
      "release of Letraset sheets containing Lorem Ipsum "
      "passages, and more recently with desktop publishing "
      "software like Aldus PageMaker including versions of "
      "Lorem Ipsum."
      "</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 2u);
  CheckTextNode(*root.children_nodes[0],
                "Lorem Ipsum is simply dummy text of the printing and "
                "typesetting industry.");
  CheckTextNode(
      *root.children_nodes[1],
      "Lorem Ipsum has been the industry's standard dummy text ever since the "
      "1500s, when an unknown printer took a galley of type and scrambled it "
      "to make a type specimen book. It has survived not only five centuries, "
      "but also the leap into electronic typesetting, remaining essentially "
      "unchanged. It was popularised in the 1960s with the release of Letraset "
      "sheets containing Lorem Ipsum passages, and more recently with desktop "
      "publishing software like Aldus PageMaker including versions of Lorem "
      "Ipsum.");
}

TEST_F(AIPageContentAgentTest, VisibilityHiddenOnSubtree) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    header {"
      "      visibility: hidden"
      "    }"
      "  </style>"
      "  <header>text</header>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 0u);
}

TEST_F(AIPageContentAgentTest, VisibilityHiddenOnParentOnly) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    #parent {"
      "      visibility: hidden"
      "    }"
      "    #child {"
      "      visibility: visible"
      "    }"
      "  </style>"
      "  <header id=parent><div id=child>text</div></header>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& container = *root.children_nodes[0];
  CheckContainerNode(container);
  EXPECT_EQ(container.children_nodes.size(), 1u);

  const auto& text_node = *container.children_nodes[0];
  CheckTextNode(text_node, "text");
}

TEST_F(AIPageContentAgentTest, VisibilityHiddenOnIframe) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    iframe {"
      "      visibility: hidden;"
      "    }"
      "  </style>"
      "  <iframe srcdoc='<div style='visibility: visible'>hidden "
      "text</div>'></iframe>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 0u);
}

TEST_F(AIPageContentAgentTest, NoGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <div>text</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  mojom::blink::AIPageContentOptions options;
  options.include_geometry = false;
  auto content = GetAIPageContent(options);
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);
  EXPECT_FALSE(content->root_node->content_attributes->geometry);

  EXPECT_EQ(content->root_node->children_nodes.size(), 1u);
  const auto& text_node = *content->root_node->children_nodes[0];
  CheckTextNode(text_node, "text");
  EXPECT_FALSE(text_node.content_attributes->geometry);
}

TEST_F(AIPageContentAgentTest, NoHiddenButSearchableContent) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    body {"
      "      margin: 0; font-size: 100px;"
      "    }"
      "  </style>"
      "  <header hidden=until-found>hidden text</header><div>visible text</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  mojom::blink::AIPageContentOptions options;
  options.include_hidden_searchable_content = false;
  auto content = GetAIPageContent(options);
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  EXPECT_EQ(content->root_node->children_nodes.size(), 2u);
  const auto& hidden_container = *content->root_node->children_nodes[0];
  CheckContainerNode(hidden_container);
  EXPECT_TRUE(hidden_container.children_nodes.empty());

  const auto& text_node = *content->root_node->children_nodes[1];
  CheckTextNode(text_node, "visible text");
}

TEST_F(AIPageContentAgentTest, FormWithTextInput) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <form name='myform'>"
      "    <label for='input1'>Lorem Ipsum</label>"
      "    <input type='text' id='input1' name='LI' value='Lorem'>"
      "    <label for='input2'>Ipsum Dolor</label>"
      "    <input type='text' id='input2' name='ID' value='Ipsum' required>"
      "  </form>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& form = *root.children_nodes[0];
  EXPECT_EQ(form.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kForm);
  EXPECT_EQ(form.content_attributes->form_data->form_name, "myform");
  EXPECT_EQ(form.children_nodes.size(), 4u);

  CheckTextNode(*form.children_nodes[0], "Lorem Ipsum");

  const auto& text_input1 = *form.children_nodes[1];
  CheckFormControlNode(text_input1, mojom::blink::FormControlType::kInputText);
  EXPECT_EQ(text_input1.content_attributes->form_control_data->field_name,
            "LI");
  EXPECT_EQ(text_input1.content_attributes->form_control_data->field_value,
            "Lorem");
  EXPECT_FALSE(text_input1.content_attributes->form_control_data->is_required);
  EXPECT_EQ(text_input1.children_nodes.size(), 1u);
  CheckContainerNode(*text_input1.children_nodes[0]);
  EXPECT_EQ(text_input1.children_nodes[0]->children_nodes.size(), 1u);
  CheckTextNode(*text_input1.children_nodes[0]->children_nodes[0], "Lorem");

  CheckTextNode(*form.children_nodes[2], "Ipsum Dolor");

  const auto& text_input2 = *form.children_nodes[3];
  CheckFormControlNode(text_input2, mojom::blink::FormControlType::kInputText);
  EXPECT_EQ(text_input2.content_attributes->form_control_data->field_name,
            "ID");
  EXPECT_EQ(text_input2.content_attributes->form_control_data->field_value,
            "Ipsum");
  EXPECT_TRUE(text_input2.content_attributes->form_control_data->is_required);
  EXPECT_EQ(text_input2.children_nodes.size(), 1u);
  CheckContainerNode(*text_input2.children_nodes[0]);
  EXPECT_EQ(text_input2.children_nodes[0]->children_nodes.size(), 1u);
  CheckTextNode(*text_input2.children_nodes[0]->children_nodes[0], "Ipsum");
}

TEST_F(AIPageContentAgentTest, FormWithSelect) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <form name='myform'>"
      "    <select name='LI'>"
      "      <option value='Lorem'>Lorem Text</option>"
      "      <option value='Ipsum'>Ipsum Text</option>"
      "    </select>"
      "  </form>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& form = *root.children_nodes[0];
  EXPECT_EQ(form.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kForm);
  EXPECT_EQ(form.content_attributes->form_data->form_name, "myform");
  EXPECT_EQ(form.children_nodes.size(), 1u);

  const auto& select = *form.children_nodes[0];
  CheckFormControlNode(select, mojom::blink::FormControlType::kSelectOne);

  const auto& select_options =
      select.content_attributes->form_control_data->select_options;
  ASSERT_EQ(select_options.size(), 2u);

  EXPECT_EQ(select_options[0]->value, "Lorem");
  EXPECT_EQ(select_options[0]->text, "Lorem Text");
  EXPECT_TRUE(select_options[0]->is_selected);

  EXPECT_EQ(select_options[1]->value, "Ipsum");
  EXPECT_EQ(select_options[1]->text, "Ipsum Text");
  EXPECT_FALSE(select_options[1]->is_selected);

  EXPECT_EQ(select.children_nodes.size(), 1u);
  CheckTextNode(*select.children_nodes[0], "Lorem Text");
}

TEST_F(AIPageContentAgentTest, FormWithCheckbox) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <form name='vehicles'>"
      "    <input type='checkbox' id='vehicle1' name='vehicle1' value='Bike'>"
      "    <label for='vehicle1'>I have a bike</label><br>"
      "    <input type='checkbox' id='vehicle2' name='vehicle2' value='Car' "
      "     checked>"
      "    <label for='vehicle2'>I have a car</label><br>"
      "  </form>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& form = *root.children_nodes[0];
  EXPECT_EQ(form.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kForm);
  EXPECT_EQ(form.content_attributes->form_data->form_name, "vehicles");
  EXPECT_EQ(form.children_nodes.size(), 4u);

  const auto& checkbox1 = *form.children_nodes[0];
  CheckFormControlNode(checkbox1,
                       mojom::blink::FormControlType::kInputCheckbox);
  EXPECT_EQ(checkbox1.content_attributes->form_control_data->field_name,
            "vehicle1");
  EXPECT_EQ(checkbox1.content_attributes->form_control_data->field_value,
            "Bike");
  EXPECT_FALSE(checkbox1.content_attributes->form_control_data->is_checked);
  EXPECT_EQ(checkbox1.children_nodes.size(), 0u);

  CheckTextNode(*form.children_nodes[1], "I have a bike");

  const auto& checkbox2 = *form.children_nodes[2];
  CheckFormControlNode(checkbox2,
                       mojom::blink::FormControlType::kInputCheckbox);
  EXPECT_EQ(checkbox2.content_attributes->form_control_data->field_name,
            "vehicle2");
  EXPECT_EQ(checkbox2.content_attributes->form_control_data->field_value,
            "Car");
  EXPECT_TRUE(checkbox2.content_attributes->form_control_data->is_checked);
  EXPECT_EQ(checkbox2.children_nodes.size(), 0u);

  CheckTextNode(*form.children_nodes[3], "I have a car");
}

TEST_F(AIPageContentAgentTest, FormWithRadio) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <form name='vehicles'>"
      "    <input type='radio' id='vehicle1' name='vehicle1' value='Bike'>"
      "    <label for='vehicle1'>I have a bike</label><br>"
      "    <input type='radio' id='vehicle2' name='vehicle2' value='Car' "
      "     checked>"
      "    <label for='vehicle2'>I have a car</label><br>"
      "  </form>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& form = *root.children_nodes[0];
  EXPECT_EQ(form.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kForm);
  EXPECT_EQ(form.content_attributes->form_data->form_name, "vehicles");
  EXPECT_EQ(form.children_nodes.size(), 4u);

  const auto& radio1 = *form.children_nodes[0];
  CheckFormControlNode(radio1, mojom::blink::FormControlType::kInputRadio);
  EXPECT_EQ(radio1.content_attributes->form_control_data->field_name,
            "vehicle1");
  EXPECT_EQ(radio1.content_attributes->form_control_data->field_value, "Bike");
  EXPECT_FALSE(radio1.content_attributes->form_control_data->is_checked);
  EXPECT_EQ(radio1.children_nodes.size(), 0u);

  CheckTextNode(*form.children_nodes[1], "I have a bike");

  const auto& radio2 = *form.children_nodes[2];
  CheckFormControlNode(radio2, mojom::blink::FormControlType::kInputRadio);
  EXPECT_EQ(radio2.content_attributes->form_control_data->field_name,
            "vehicle2");
  EXPECT_EQ(radio2.content_attributes->form_control_data->field_value, "Car");
  EXPECT_TRUE(radio2.content_attributes->form_control_data->is_checked);
  EXPECT_EQ(radio2.children_nodes.size(), 0u);

  CheckTextNode(*form.children_nodes[3], "I have a car");
}

TEST_F(AIPageContentAgentTest, InteractiveElements) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    div {"
      "      resize: both;"
      "      overflow: auto;"
      "      border: 1px solid black;"
      "      width: 200px;"
      "    }"
      "  </style>"
      "  <textarea>text</textarea>"
      "  <button>button</button>"
      "  <div>resize</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.children_nodes.size(), 3u);

  const auto& text_area = *root.children_nodes[0];
  CheckFormControlNode(text_area, mojom::blink::FormControlType::kTextArea);
  EXPECT_TRUE(
      text_area.content_attributes->node_interaction_info->is_selectable);
  EXPECT_FALSE(
      text_area.content_attributes->node_interaction_info->is_editable);
  EXPECT_TRUE(
      text_area.content_attributes->node_interaction_info->is_focusable);
  EXPECT_FALSE(
      text_area.content_attributes->node_interaction_info->is_draggable);
  EXPECT_TRUE(
      text_area.content_attributes->node_interaction_info->is_clickable);
  EXPECT_TRUE(
      text_area.content_attributes->node_interaction_info->can_resize_vertical);
  EXPECT_TRUE(text_area.content_attributes->node_interaction_info
                  ->can_resize_horizontal);

  EXPECT_EQ(text_area.children_nodes.size(), 1u);
  const auto& text_area_text = *text_area.children_nodes[0];
  CheckTextNode(text_area_text, "text");
  EXPECT_TRUE(
      text_area_text.content_attributes->node_interaction_info->is_selectable);
  EXPECT_TRUE(
      text_area_text.content_attributes->node_interaction_info->is_editable);
  EXPECT_FALSE(
      text_area_text.content_attributes->node_interaction_info->is_focusable);
  EXPECT_FALSE(
      text_area_text.content_attributes->node_interaction_info->is_draggable);
  EXPECT_FALSE(
      text_area_text.content_attributes->node_interaction_info->is_clickable);
  EXPECT_FALSE(text_area_text.content_attributes->node_interaction_info
                   ->can_resize_vertical);
  EXPECT_FALSE(text_area_text.content_attributes->node_interaction_info
                   ->can_resize_horizontal);

  const auto& button = *root.children_nodes[1];
  CheckFormControlNode(button, mojom::blink::FormControlType::kButtonSubmit);
  EXPECT_TRUE(button.content_attributes->node_interaction_info->is_selectable);
  EXPECT_FALSE(button.content_attributes->node_interaction_info->is_editable);
  EXPECT_TRUE(button.content_attributes->node_interaction_info->is_focusable);
  EXPECT_FALSE(button.content_attributes->node_interaction_info->is_draggable);
  EXPECT_TRUE(button.content_attributes->node_interaction_info->is_clickable);
  EXPECT_FALSE(
      button.content_attributes->node_interaction_info->can_resize_vertical);
  EXPECT_FALSE(
      button.content_attributes->node_interaction_info->can_resize_horizontal);

  EXPECT_EQ(button.children_nodes.size(), 1u);
  const auto& button_text = *button.children_nodes[0];
  CheckTextNode(button_text, "button");
  EXPECT_TRUE(
      button_text.content_attributes->node_interaction_info->is_selectable);
  EXPECT_FALSE(
      button_text.content_attributes->node_interaction_info->is_editable);
  EXPECT_FALSE(
      button_text.content_attributes->node_interaction_info->is_focusable);
  EXPECT_FALSE(
      button_text.content_attributes->node_interaction_info->is_draggable);
  EXPECT_FALSE(
      button_text.content_attributes->node_interaction_info->is_clickable);
  EXPECT_FALSE(button_text.content_attributes->node_interaction_info
                   ->can_resize_vertical);
  EXPECT_FALSE(button_text.content_attributes->node_interaction_info
                   ->can_resize_horizontal);

  const auto& resize = *root.children_nodes[2];
  CheckContainerNode(resize);
  EXPECT_TRUE(resize.content_attributes->node_interaction_info->is_selectable);
  EXPECT_FALSE(resize.content_attributes->node_interaction_info->is_editable);
  EXPECT_FALSE(resize.content_attributes->node_interaction_info->is_focusable);
  EXPECT_FALSE(resize.content_attributes->node_interaction_info->is_draggable);
  EXPECT_FALSE(resize.content_attributes->node_interaction_info->is_clickable);
  EXPECT_TRUE(
      resize.content_attributes->node_interaction_info->can_resize_vertical);
  EXPECT_TRUE(
      resize.content_attributes->node_interaction_info->can_resize_horizontal);

  EXPECT_EQ(resize.children_nodes.size(), 1u);
  const auto& resize_text = *resize.children_nodes[0];
  CheckTextNode(resize_text, "resize");
  EXPECT_TRUE(
      resize_text.content_attributes->node_interaction_info->is_selectable);
  EXPECT_FALSE(
      resize_text.content_attributes->node_interaction_info->is_editable);
  EXPECT_FALSE(
      resize_text.content_attributes->node_interaction_info->is_focusable);
  EXPECT_FALSE(
      resize_text.content_attributes->node_interaction_info->is_draggable);
  EXPECT_FALSE(
      resize_text.content_attributes->node_interaction_info->is_clickable);
  EXPECT_FALSE(resize_text.content_attributes->node_interaction_info
                   ->can_resize_vertical);
  EXPECT_FALSE(resize_text.content_attributes->node_interaction_info
                   ->can_resize_horizontal);
}

TEST_F(AIPageContentAgentTest, ContentNodeIds) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <main>"
      "    <h1>Heading</h1>"
      "    text"
      "  </main>"
      "  <iframe srcdoc='iframe text'></iframe>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.content_attributes->content_node_id, 0);
  EXPECT_EQ(root.children_nodes.size(), 2u);

  const auto& main = *root.children_nodes[0];
  CheckContainerNode(main);
  EXPECT_EQ(main.content_attributes->content_node_id, 1);
  EXPECT_EQ(main.children_nodes.size(), 2u);

  auto& heading = *main.children_nodes[0];
  CheckHeadingNode(heading);
  EXPECT_EQ(heading.content_attributes->content_node_id, 2);
  EXPECT_EQ(heading.children_nodes.size(), 1u);

  CheckTextNode(*heading.children_nodes[0], "Heading");
  EXPECT_EQ(heading.children_nodes[0]->content_attributes->content_node_id, 3);

  CheckTextNode(*main.children_nodes[1], "    text  ");
  EXPECT_EQ(main.children_nodes[1]->content_attributes->content_node_id, 4);

  const auto& iframe = *root.children_nodes[1];
  CheckIframeNode(iframe);
  EXPECT_EQ(iframe.content_attributes->content_node_id, 7);
  EXPECT_EQ(iframe.children_nodes.size(), 1u);

  const auto& iframe_root = *iframe.children_nodes[0];
  CheckRootNode(iframe_root);
  EXPECT_EQ(iframe_root.content_attributes->content_node_id, 5);
  EXPECT_EQ(iframe_root.children_nodes.size(), 1u);

  CheckTextNode(*iframe_root.children_nodes[0], "iframe text");
  EXPECT_EQ(iframe_root.children_nodes[0]->content_attributes->content_node_id,
            6);
}

TEST_F(AIPageContentAgentTest, Selection) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <p id='p1'>Paragraph 1</p>"
      "  <p id='p2'>Paragraph 2</p>"
      "  <p id='p3'>Paragraph 3</p>"
      "  <script>"
      "    const p1 = document.getElementById('p1');"
      "    const p2 = document.getElementById('p2');"
      "    const range = new Range();"
      "    range.setStart(p1.childNodes[0], 10);"
      "    range.setEnd(p2.childNodes[0], 9);"
      "    const selection = window.getSelection();"
      "    selection.removeAllRanges();"
      "    selection.addRange(range);"
      "  </script>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.content_attributes->content_node_id, 0);
  EXPECT_EQ(root.children_nodes.size(), 3u);

  const auto& paragraph1 = *root.children_nodes[0];
  EXPECT_EQ(paragraph1.content_attributes->content_node_id, 1);
  CheckTextNode(*paragraph1.children_nodes[0], "Paragraph 1");
  EXPECT_EQ(paragraph1.children_nodes[0]->content_attributes->content_node_id,
            2);

  const auto& paragraph2 = *root.children_nodes[1];
  EXPECT_EQ(paragraph2.content_attributes->content_node_id, 3);
  CheckTextNode(*paragraph2.children_nodes[0], "Paragraph 2");
  EXPECT_EQ(paragraph2.children_nodes[0]->content_attributes->content_node_id,
            4);

  const auto& paragraph3 = *root.children_nodes[2];
  EXPECT_EQ(paragraph3.content_attributes->content_node_id, 5);
  CheckTextNode(*paragraph3.children_nodes[0], "Paragraph 3");
  EXPECT_EQ(paragraph3.children_nodes[0]->content_attributes->content_node_id,
            6);

  const auto& frame_interaction_info = content->main_frame_interaction_info;
  ASSERT_TRUE(frame_interaction_info->selection);
  const auto& selection = *frame_interaction_info->selection;
  EXPECT_EQ(selection.selected_text, "1\n\nParagraph");
  EXPECT_EQ(selection.start_node_id, 2);
  EXPECT_EQ(selection.end_node_id, 4);
  EXPECT_EQ(selection.start_offset, 10);
  EXPECT_EQ(selection.end_offset, 9);
}

TEST_F(AIPageContentAgentTest, SelectionInIframe) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <iframe srcdoc='"
      "    <p id=\"p1\">Paragraph 1</p>"
      "    <p id=\"p2\">Paragraph 2</p>"
      "    <p id=\"p3\">Paragraph 3</p>"
      "    <script>"
      "      const p1 = document.getElementById(\"p1\");"
      "      const p2 = document.getElementById(\"p2\");"
      "      const range = new Range();"
      "      range.setStart(p1.childNodes[0], 10);"
      "      range.setEnd(p2.childNodes[0], 9);"
      "      const selection = window.getSelection();"
      "      selection.removeAllRanges();"
      "      selection.addRange(range);"
      "    </script>"
      "  '></iframe>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.content_attributes->content_node_id, 0);
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& iframe = *root.children_nodes[0];
  CheckIframeNode(iframe);
  EXPECT_EQ(iframe.content_attributes->content_node_id, 8);
  EXPECT_EQ(iframe.children_nodes.size(), 1u);

  const auto& iframe_root = *iframe.children_nodes[0];
  CheckRootNode(iframe_root);
  EXPECT_EQ(iframe_root.content_attributes->content_node_id, 1);
  EXPECT_EQ(iframe_root.children_nodes.size(), 3u);

  const auto& paragraph1 = *iframe_root.children_nodes[0];
  EXPECT_EQ(paragraph1.content_attributes->content_node_id, 2);
  CheckTextNode(*paragraph1.children_nodes[0], "Paragraph 1");
  EXPECT_EQ(paragraph1.children_nodes[0]->content_attributes->content_node_id,
            3);

  const auto& paragraph2 = *iframe_root.children_nodes[1];
  EXPECT_EQ(paragraph2.content_attributes->content_node_id, 4);
  CheckTextNode(*paragraph2.children_nodes[0], "Paragraph 2");
  EXPECT_EQ(paragraph2.children_nodes[0]->content_attributes->content_node_id,
            5);

  const auto& paragraph3 = *iframe_root.children_nodes[2];
  EXPECT_EQ(paragraph3.content_attributes->content_node_id, 6);
  CheckTextNode(*paragraph3.children_nodes[0], "Paragraph 3");
  EXPECT_EQ(paragraph3.children_nodes[0]->content_attributes->content_node_id,
            7);

  const auto& main_frame_interaction_info =
      content->main_frame_interaction_info;
  ASSERT_FALSE(main_frame_interaction_info->selection);

  const auto& iframe_interaction_info =
      iframe.content_attributes->iframe_data->frame_interaction_info;
  ASSERT_TRUE(iframe_interaction_info->selection);
  const auto& selection = *iframe_interaction_info->selection;
  EXPECT_EQ(selection.selected_text, "1\n\nParagraph");
  EXPECT_EQ(selection.start_node_id, 3);
  EXPECT_EQ(selection.end_node_id, 5);
  EXPECT_EQ(selection.start_offset, 10);
  EXPECT_EQ(selection.end_offset, 9);
}

TEST_F(AIPageContentAgentTest, Focus) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <button id='button'>button</button>"
      "  <script>"
      "    const button = document.getElementById('button');"
      "    button.focus();"
      "  </script>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.content_attributes->content_node_id, 0);
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& button = *root.children_nodes[0];
  EXPECT_EQ(button.content_attributes->content_node_id, 1);

  const auto& page_interaction_info = content->page_interaction_info;
  EXPECT_EQ(page_interaction_info->focused_node_id, 1);
}

TEST_F(AIPageContentAgentTest, MousePosition) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    div {"
      "      position: absolute;"
      "      top: 100px;"
      "      left: 200px;"
      "    }"
      "  </style>"
      "  <div>text</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  // Move the mouse to the middle of the page.
  FireMouseMoveEvent(gfx::PointF(150, 50));

  auto content = GetAIPageContent();
  ASSERT_TRUE(content);
  ASSERT_TRUE(content->root_node);

  const auto& root = *content->root_node;
  EXPECT_EQ(root.content_attributes->content_node_id, 0);
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& text = *root.children_nodes[0];
  CheckTextNode(text, "text");
  EXPECT_EQ(text.content_attributes->content_node_id, 1);

  EXPECT_EQ(content->page_interaction_info->mouse_position->x(), 150);
  EXPECT_EQ(content->page_interaction_info->mouse_position->y(), 50);
}

}  // namespace
}  // namespace blink
