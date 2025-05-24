// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include <cstddef>

#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

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

  void GetAIPageContentWithActionableElements() {
    auto options = GetAIPageContentOptionsForTest();
    options.enable_experimental_actionable_data = true;
    GetAIPageContent(options);
  }

  static mojom::blink::AIPageContentOptions GetAIPageContentOptionsForTest() {
    mojom::blink::AIPageContentOptions options;
    options.include_geometry = true;
    options.on_critical_path = true;
    options.include_hidden_searchable_content = true;
    return options;
  }

  void GetAIPageContent(std::optional<mojom::blink::AIPageContentOptions>
                            options = std::nullopt) {
    auto* agent = AIPageContentAgent::GetOrCreateForTesting(
        *helper_.LocalMainFrame()->GetFrame()->GetDocument());
    EXPECT_TRUE(agent);

    last_options_ = options ? *options : default_options_;
    auto content = agent->GetAIPageContentInternal(last_options_);
    CHECK(content);
    CHECK(content->root_node);

    // Always validate serialization.
    mojom::blink::AIPageContentPtr output;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::blink::AIPageContent>(
            content, output));

    last_content_ = std::move(content);
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

  const mojom::blink::AIPageContentNode& ContentRootNode() {
    CHECK(last_content_);

    EXPECT_TRUE(last_content_->root_node);
    if (!last_options_.enable_experimental_actionable_data) {
      return *last_content_->root_node;
    }

    EXPECT_EQ(last_content_->root_node->children_nodes.size(), 1u);
    const auto& html = *last_content_->root_node->children_nodes[0];

    EXPECT_EQ(html.children_nodes.size(), 1u);
    return *html.children_nodes[0];
  }

  void CheckHitTestableButNotInteractive(
      const mojom::blink::AIPageContentNode& node) {
    CHECK(node.content_attributes->node_interaction_info);
    EXPECT_TRUE(node.content_attributes->node_interaction_info
                    ->document_scoped_z_order);
    EXPECT_FALSE(node.content_attributes->node_interaction_info->is_clickable);
  }

  const mojom::blink::AIPageContentPtr& Content() { return last_content_; }

 protected:
  const mojom::blink::AIPageContentOptions default_options_ =
      GetAIPageContentOptionsForTest();
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;

 private:
  static void UpdateWebSettings(WebSettings* settings) {
    settings->SetTextAreasAreResizable(true);
  }

  mojom::blink::AIPageContentPtr last_content_;
  mojom::blink::AIPageContentOptions last_options_;
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& attributes = *root.content_attributes;
  EXPECT_TRUE(attributes.dom_node_id.has_value());

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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  auto& image_node = *root.children_nodes[0];
  CheckImageNode(image_node, "missing");
  CheckGeometry(image_node, gfx::Rect(-20, -10, 30, 40),
                gfx::Rect(0, 0, 10, 30));
}

TEST_F(AIPageContentAgentTest, ImageWithAriaLabel) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <img aria-label='hello'></img>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  auto& image_node = *root.children_nodes[0];
  EXPECT_EQ("hello", image_node.content_attributes->label);
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

  GetAIPageContent();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_TRUE(root.children_nodes.empty());
}

TEST_F(AIPageContentAgentTest, VisibilityHidden) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <div style='visibility: hidden;'>Hidden Content</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 3u);

  EXPECT_FALSE(root.content_attributes->geometry->is_fixed_or_sticky_position);

  const auto& fixed_element = *root.children_nodes[0];
  CheckContainerNode(fixed_element);
  EXPECT_TRUE(
      fixed_element.content_attributes->geometry->is_fixed_or_sticky_position);
  EXPECT_FALSE(fixed_element.content_attributes->node_interaction_info);
  CheckTextNode(*fixed_element.children_nodes[0],
                "This element stays in place when the page is scrolled.");

  const auto& sticky_element = *root.children_nodes[1];
  CheckContainerNode(sticky_element);
  EXPECT_TRUE(
      sticky_element.content_attributes->geometry->is_fixed_or_sticky_position);
  EXPECT_FALSE(sticky_element.content_attributes->node_interaction_info);
  CheckTextNode(*sticky_element.children_nodes[0],
                "This element stays in place when the page is scrolled.");

  const auto& normal_element = *root.children_nodes[2];
  EXPECT_FALSE(
      normal_element.content_attributes->geometry->is_fixed_or_sticky_position);
  EXPECT_FALSE(normal_element.content_attributes->node_interaction_info);
  CheckTextNode(normal_element,
                "This element flows naturally with the document.");
}

TEST_F(AIPageContentAgentTest, RootScroller) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
        <body style='margin: 0px;'>
          <div style='width: 200vw; height: 300vh; background: grey;'></div>
          <script>
            document.scrollingElement.scrollTop=100;
            document.scrollingElement.scrollLeft=200;
           </script>
        </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& root = ContentRootNode();
  ASSERT_TRUE(root.content_attributes->node_interaction_info);
  ASSERT_TRUE(root.content_attributes->node_interaction_info->scroller_info);

  const auto& root_scroller =
      *root.content_attributes->node_interaction_info->scroller_info;
  EXPECT_EQ(root_scroller.scrolling_bounds.width(), 2 * kWindowSize.width());
  EXPECT_EQ(root_scroller.scrolling_bounds.height(), 3 * kWindowSize.height());

  EXPECT_EQ(root_scroller.visible_area,
            gfx::Rect(200, 100, kWindowSize.width(), kWindowSize.height()));
}

class AIPageContentAgentTestWithSubScroller
    : public AIPageContentAgentTest,
      public testing::WithParamInterface<std::string> {};

TEST_P(AIPageContentAgentTestWithSubScroller, Overflow) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      base::StringPrintf(
          R"HTML(
          <body style='margin: 0px;'>
            <style>
             #scroller {
               overflow:%s; width: 100vw; height:100vh;
               position:relative; top: 30px; left:50px;
             }
            </style>
            <div id='scroller'>
             <div style='width: 200vw; height: 300vh; background: grey;'></div>
            </div>
            <script>
              let scroller = document.getElementById('scroller');
              scroller.scrollTop=100;
              scroller.scrollLeft=200;
             </script>
          </body>
          )HTML",
          GetParam()),
      url_test_helpers::ToKURL("http://foobar.com"));

  SCOPED_TRACE(GetParam());
  GetAIPageContent();

  const auto& root = ContentRootNode();
  ASSERT_TRUE(root.content_attributes->node_interaction_info);
  ASSERT_TRUE(root.content_attributes->node_interaction_info->scroller_info);

  const auto& root_scroller =
      *root.content_attributes->node_interaction_info->scroller_info;
  EXPECT_EQ(root_scroller.scrolling_bounds.width(), kWindowSize.width() + 50);
  EXPECT_EQ(root_scroller.scrolling_bounds.height(), kWindowSize.height() + 30);
  EXPECT_EQ(root_scroller.visible_area, gfx::Rect(kWindowSize));

  ASSERT_EQ(root.children_nodes.size(), 1u);
  const auto& child = *root.children_nodes.at(0);
  ASSERT_TRUE(child.content_attributes->node_interaction_info);
  ASSERT_TRUE(child.content_attributes->node_interaction_info->scroller_info);

  const auto& sub_scroller =
      *child.content_attributes->node_interaction_info->scroller_info;
  EXPECT_EQ(sub_scroller.scrolling_bounds.width(), 2 * kWindowSize.width());
  EXPECT_EQ(sub_scroller.scrolling_bounds.height(), 3 * kWindowSize.height());

  EXPECT_EQ(sub_scroller.visible_area,
            gfx::Rect(200, 100, kWindowSize.width(), kWindowSize.height()));

  bool user_scrollable = GetParam() != "hidden";
  EXPECT_EQ(sub_scroller.user_scrollable_horizontal, user_scrollable);
  EXPECT_EQ(sub_scroller.user_scrollable_vertical, user_scrollable);
}

INSTANTIATE_TEST_SUITE_P(AIPageContentAgentTestWithSubScroller,
                         AIPageContentAgentTestWithSubScroller,
                         ::testing::Values("auto", "scroll", "hidden"));

TEST_F(AIPageContentAgentTest, OverflowVisible) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body style='margin: 0px;'>
        <style>
         #scroller {
           overflow:visible; width: 100vw; height:100vh;
           position:relative; top: 30px; left:50px;
         }
        </style>
        <div id='scroller'>
         <div style='width: 200vw; height: 300vh; background: grey;'></div>
        </div>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& root = ContentRootNode();
  ASSERT_TRUE(root.content_attributes->node_interaction_info);
  ASSERT_TRUE(root.content_attributes->node_interaction_info->scroller_info);

  const auto& root_scroller =
      *root.content_attributes->node_interaction_info->scroller_info;
  EXPECT_EQ(root_scroller.scrolling_bounds.width(),
            kWindowSize.width() * 2 + 50);
  EXPECT_EQ(root_scroller.scrolling_bounds.height(),
            kWindowSize.height() * 3 + 30);
  EXPECT_EQ(root_scroller.visible_area, gfx::Rect(kWindowSize));

  EXPECT_EQ(root.children_nodes.size(), 0u);
}

TEST_F(AIPageContentAgentTest, OverflowClip) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body style='margin: 0px;'>
        <style>
         #scroller {
           overflow:clip; width: 100vw; height:100vh;
           position:relative; top: 30px; left:50px;
         }
        </style>
        <div id='scroller'>
         <div style='width: 200vw; height: 300vh; background: grey;'></div>
        </div>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& root = ContentRootNode();
  ASSERT_TRUE(root.content_attributes->node_interaction_info);
  ASSERT_TRUE(root.content_attributes->node_interaction_info->scroller_info);

  const auto& root_scroller =
      *root.content_attributes->node_interaction_info->scroller_info;
  EXPECT_EQ(root_scroller.scrolling_bounds.width(), kWindowSize.width() + 50);
  EXPECT_EQ(root_scroller.scrolling_bounds.height(), kWindowSize.height() + 30);
  EXPECT_EQ(root_scroller.visible_area, gfx::Rect(kWindowSize));

  EXPECT_EQ(root.children_nodes.size(), 0u);
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  // Two nodes: the dialog and its backdrop.
  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& text_node = *root.children_nodes[0];
  CheckTextNode(text_node, "far text");

  const auto& attributes = *text_node.content_attributes;
  EXPECT_TRUE(attributes.dom_node_id.has_value());

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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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
  GetAIPageContent(options);

  const auto& root = ContentRootNode();
  EXPECT_FALSE(root.content_attributes->geometry);

  EXPECT_EQ(root.children_nodes.size(), 1u);
  const auto& text_node = *root.children_nodes[0];
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
  GetAIPageContent(options);

  const auto& root = ContentRootNode();

  EXPECT_EQ(root.children_nodes.size(), 2u);
  const auto& hidden_container = *root.children_nodes[0];
  CheckContainerNode(hidden_container);
  EXPECT_TRUE(hidden_container.children_nodes.empty());

  const auto& text_node = *root.children_nodes[1];
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
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

TEST_F(AIPageContentAgentTest, FormWithPassword) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <form>"
      "    <input type='password' name='Enter password' value='mypassword'>"
      "  </form>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto* agent = AIPageContentAgent::GetOrCreateForTesting(
      *helper_.LocalMainFrame()->GetFrame()->GetDocument());
  ASSERT_TRUE(agent);

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& form = *root.children_nodes[0];
  EXPECT_EQ(form.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kForm);
  EXPECT_EQ(form.children_nodes.size(), 1u);

  const auto& password = *form.children_nodes[0];
  CheckFormControlNode(password, mojom::blink::FormControlType::kInputPassword);
  EXPECT_EQ(password.content_attributes->form_control_data->field_name,
            "Enter password");
  EXPECT_EQ(password.content_attributes->form_control_data->field_value,
            nullptr);
  EXPECT_EQ(password.children_nodes.size(), 1u);
  CheckContainerNode(*password.children_nodes[0]);
  EXPECT_EQ(password.children_nodes[0]->children_nodes.size(), 1u);
  CheckTextNode(*password.children_nodes[0]->children_nodes[0], u"••••••••••");
}

TEST_F(AIPageContentAgentTest, InteractiveElementsTextArea) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <textarea>text</textarea>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

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

  // Text area uses a UA shadow DOM internally to create an editable box.
  const auto& shadow_div = *text_area.children_nodes[0];
  EXPECT_EQ(shadow_div.children_nodes.size(), 1u);
  CheckContainerNode(shadow_div);
  EXPECT_TRUE(
      shadow_div.content_attributes->node_interaction_info->is_selectable);
  EXPECT_TRUE(
      shadow_div.content_attributes->node_interaction_info->is_editable);
  EXPECT_FALSE(
      shadow_div.content_attributes->node_interaction_info->is_focusable);
  EXPECT_FALSE(
      shadow_div.content_attributes->node_interaction_info->is_draggable);
  EXPECT_TRUE(
      shadow_div.content_attributes->node_interaction_info->is_clickable);
  EXPECT_FALSE(shadow_div.content_attributes->node_interaction_info
                   ->can_resize_vertical);
  EXPECT_FALSE(shadow_div.content_attributes->node_interaction_info
                   ->can_resize_horizontal);

  EXPECT_EQ(shadow_div.children_nodes.size(), 1u);
  const auto& text_area_text = *shadow_div.children_nodes[0];
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
}

TEST_F(AIPageContentAgentTest, InteractiveElementsButton) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <button>button</button>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& button = *root.children_nodes[0];
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

  ASSERT_EQ(button.children_nodes.size(), 1u);
  const auto& button_text = *button.children_nodes[0];
  CheckTextNode(button_text, "button");
  EXPECT_TRUE(button_text.content_attributes->node_interaction_info);
  EXPECT_FALSE(
      button_text.content_attributes->node_interaction_info->is_clickable);
}

TEST_F(AIPageContentAgentTest, InteractiveElementsResizableDiv) {
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
      "  <div>resize</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& resize = *ContentRootNode().children_nodes[0];
  CheckContainerNode(resize);
  ASSERT_TRUE(resize.content_attributes->node_interaction_info);
  EXPECT_FALSE(resize.content_attributes->node_interaction_info->scroller_info);
  EXPECT_TRUE(resize.content_attributes->node_interaction_info->is_selectable);
  EXPECT_FALSE(resize.content_attributes->node_interaction_info->is_editable);
  EXPECT_FALSE(resize.content_attributes->node_interaction_info->is_focusable);
  EXPECT_FALSE(resize.content_attributes->node_interaction_info->is_draggable);
  EXPECT_FALSE(resize.content_attributes->node_interaction_info->is_clickable);
  EXPECT_TRUE(
      resize.content_attributes->node_interaction_info->can_resize_vertical);
  EXPECT_TRUE(
      resize.content_attributes->node_interaction_info->can_resize_horizontal);

  ASSERT_EQ(resize.children_nodes.size(), 1u);
  const auto& resize_text = *resize.children_nodes[0];
  CheckTextNode(resize_text, "resize");
  EXPECT_TRUE(resize_text.content_attributes->node_interaction_info);
  EXPECT_FALSE(
      resize_text.content_attributes->node_interaction_info->is_clickable);
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 3u);

  const auto& paragraph1 = *root.children_nodes[0];
  CheckTextNode(*paragraph1.children_nodes[0], "Paragraph 1");

  const auto& paragraph2 = *root.children_nodes[1];
  CheckTextNode(*paragraph2.children_nodes[0], "Paragraph 2");

  const auto& paragraph3 = *root.children_nodes[2];
  CheckTextNode(*paragraph3.children_nodes[0], "Paragraph 3");

  const auto& frame_interaction_info =
      Content()->frame_data->frame_interaction_info;
  ASSERT_TRUE(frame_interaction_info->selection);
  const auto& selection = *frame_interaction_info->selection;
  EXPECT_EQ(selection.selected_text, "1\n\nParagraph");
  EXPECT_EQ(selection.start_dom_node_id,
            paragraph1.children_nodes[0]->content_attributes->dom_node_id);
  EXPECT_EQ(selection.end_dom_node_id,
            paragraph2.children_nodes[0]->content_attributes->dom_node_id);
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& iframe = *root.children_nodes[0];
  CheckIframeNode(iframe);
  EXPECT_EQ(iframe.children_nodes.size(), 1u);

  const auto& iframe_root = *iframe.children_nodes[0];
  CheckRootNode(iframe_root);
  EXPECT_EQ(iframe_root.children_nodes.size(), 3u);

  const auto& paragraph1 = *iframe_root.children_nodes[0];
  CheckTextNode(*paragraph1.children_nodes[0], "Paragraph 1");

  const auto& paragraph2 = *iframe_root.children_nodes[1];
  CheckTextNode(*paragraph2.children_nodes[0], "Paragraph 2");

  const auto& paragraph3 = *iframe_root.children_nodes[2];
  CheckTextNode(*paragraph3.children_nodes[0], "Paragraph 3");

  const auto& frame_interaction_info =
      Content()->frame_data->frame_interaction_info;
  ASSERT_FALSE(frame_interaction_info->selection);

  const auto& iframe_interaction_info =
      iframe.content_attributes->iframe_data->local_frame_data
          ->frame_interaction_info;
  ASSERT_TRUE(iframe_interaction_info->selection);
  const auto& selection = *iframe_interaction_info->selection;
  EXPECT_EQ(selection.selected_text, "1\n\nParagraph");
  EXPECT_EQ(selection.start_dom_node_id,
            paragraph1.children_nodes[0]->content_attributes->dom_node_id);
  EXPECT_EQ(selection.end_dom_node_id,
            paragraph2.children_nodes[0]->content_attributes->dom_node_id);
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& button = *root.children_nodes[0];
  const auto& page_interaction_info = Content()->page_interaction_info;
  EXPECT_EQ(page_interaction_info->focused_dom_node_id,
            button.content_attributes->dom_node_id);
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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& text = *root.children_nodes[0];
  CheckTextNode(text, "text");

  EXPECT_EQ(Content()->page_interaction_info->mouse_position->x(), 150);
  EXPECT_EQ(Content()->page_interaction_info->mouse_position->y(), 50);
}

TEST_F(AIPageContentAgentTest, MetaTags) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<head>"
      "  <meta charset='UTF-8'>"
      "  <meta content='ignored'>"
      "  <meta name='author' content='George'>"
      "  <meta name='keywords' content='HTML, CSS, JavaScript'>"
      "  <meta name='nocontent'>"
      "  <meta name='emptycontent' content=''>"
      "  <meta id='nullcontent' name='nullcontent'>"
      "</head>"
      "<body>"
      "  <meta name='ignored'>"
      "  <iframe srcdoc=\""
      "    <head>"
      "      <meta charset='UTF-8'>"
      "      <meta name='author' content='Gary'>"
      "      <meta name='keywords' content='HTML, CSS, JavaScript'>"
      "    </head>"
      "    <body>child frame</body>"
      "  \""
      "  </iframe>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  // Explicitly set the content of the nullcontent meta tag to the null atom to
  // test this case.
  auto& document = *helper_.LocalMainFrame()->GetFrame()->GetDocument();
  document.getElementById(AtomicString("nullcontent"))
      ->setAttribute(html_names::kContentAttr, WTF::g_null_atom);

  mojom::blink::AIPageContentOptions options;
  options.max_meta_elements = 32;
  GetAIPageContent(options);

  EXPECT_EQ(Content()->frame_data->meta_data.size(), 5u);

  EXPECT_EQ(Content()->frame_data->meta_data[0]->name, "author");
  EXPECT_EQ(Content()->frame_data->meta_data[0]->content, "George");

  EXPECT_EQ(Content()->frame_data->meta_data[1]->name, "keywords");
  EXPECT_EQ(Content()->frame_data->meta_data[1]->content,
            "HTML, CSS, JavaScript");

  EXPECT_EQ(Content()->frame_data->meta_data[2]->name, "nocontent");
  EXPECT_EQ(Content()->frame_data->meta_data[3]->content, "");

  EXPECT_EQ(Content()->frame_data->meta_data[3]->name, "emptycontent");
  EXPECT_EQ(Content()->frame_data->meta_data[3]->content, "");

  EXPECT_EQ(Content()->frame_data->meta_data[4]->name, "nullcontent");
  EXPECT_EQ(Content()->frame_data->meta_data[4]->content, "");

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& iframe = *root.children_nodes[0];
  EXPECT_EQ(iframe.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);

  const auto& iframe_data = *iframe.content_attributes->iframe_data;
  EXPECT_EQ(iframe_data.local_frame_data->meta_data.size(), 2u);

  EXPECT_EQ(iframe_data.local_frame_data->meta_data[0]->name, "author");
  EXPECT_EQ(iframe_data.local_frame_data->meta_data[0]->content, "Gary");

  EXPECT_EQ(iframe_data.local_frame_data->meta_data[1]->name, "keywords");
  EXPECT_EQ(iframe_data.local_frame_data->meta_data[1]->content,
            "HTML, CSS, JavaScript");
}

TEST_F(AIPageContentAgentTest, NestedIframesMetaTags) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<head><meta name=author content=George></head>"
      "<body>parent"
      "  <iframe srcdoc=\""
      "    <head><meta name=author content=Gary></head>"
      "    <body>child"
      "      <iframe srcdoc='"
      "        <head><meta name=author content=Jordan></head>"
      "        <body>grandchild</body"
      "      '></iframe>"
      "    </body>"
      "  \"></iframe>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  mojom::blink::AIPageContentOptions options;
  options.max_meta_elements = 32;
  GetAIPageContent(options);

  EXPECT_EQ(Content()->frame_data->meta_data.size(), 1u);

  EXPECT_EQ(Content()->frame_data->meta_data[0]->name, "author");
  EXPECT_EQ(Content()->frame_data->meta_data[0]->content, "George");

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 2u);

  const auto& iframe = *root.children_nodes[1];
  EXPECT_EQ(iframe.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);

  const auto& iframe_data = *iframe.content_attributes->iframe_data;
  EXPECT_EQ(iframe_data.local_frame_data->meta_data.size(), 1u);

  EXPECT_EQ(iframe_data.local_frame_data->meta_data[0]->name, "author");
  EXPECT_EQ(iframe_data.local_frame_data->meta_data[0]->content, "Gary");

  EXPECT_EQ(iframe.children_nodes.size(), 1u);

  // In the iframe children_nodes there is a root node that has two children.
  // The first child is a text node and the second is the subiframe.  The key
  // thing we want to check here is that the subiframe has the correct meta
  // data.
  const auto& subiframe = *iframe.children_nodes[0]->children_nodes[1];
  EXPECT_EQ(subiframe.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);

  const auto& subiframe_data = *subiframe.content_attributes->iframe_data;
  EXPECT_EQ(subiframe_data.local_frame_data->meta_data.size(), 1u);

  EXPECT_EQ(subiframe_data.local_frame_data->meta_data[0]->name, "author");
  EXPECT_EQ(subiframe_data.local_frame_data->meta_data[0]->content, "Jordan");
}

TEST_F(AIPageContentAgentTest, Title) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<head>"
      "  <title>test title</title>"
      "</head>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  EXPECT_EQ(Content()->frame_data->title, "test title");
}

bool ContainsRole(const Vector<mojom::blink::AIPageContentAnnotatedRole>& roles,
                  mojom::blink::AIPageContentAnnotatedRole role) {
  for (const auto& r : roles) {
    if (r == role) {
      return true;
    }
  }
  return false;
}

TEST_F(AIPageContentAgentTest, PaidContent) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
  <head>
  <script></script>
  <script type='unrelated'></script>
  <script type="application/ld+json">{this: "will fail parsing",}</script>
  <script type="application/ld+json">"not": "an object"</script>
  <script type="application/ld+json">{
    "@context": "http://schema.org",
    "@type": "NewsArticle",
    "mainEntityOfPage": "https://www.evergreengazette.com/world/world-news/",
    "headline": "City Council Debates Future of Automated Transit System",
    "alternativeHeadline": "City Council Debates Future of Automated Transit System",
    "dateModified": "2025-03-25T19:17:05.541Z",
    "datePublished": "2025-03-25T09:02:58.131Z",
    "description": "The City Council has been asked to discuss the future of automated transit systems, including the feasibility of a bus-on-rails system, in a special meeting on Thursday, March 28.",
    "author": [
        {
            "@type": "Person",
            "name": "Finlay Joy",
            "url": "https://www.evergreengazette.com/people/finlay-joy/"
        },
        {
            "@type": "Person",
            "name": "Calum Gerhard",
            "url": "https://www.evergreengazette.com/people/calum-gerhard/"
        }
    ],
    "isPartOf": {
        "@type": [
            "CreativeWork",
            "Product"
        ],
        "name": "The Evergreen Gazette",
        "productID": "evergreengazette.com:basic",
        "description": "The Evergreen Gazette is your trusted source for comprehensive local news, insightful analysis, and community-focused reporting.",
        "sku": "https://subscribe.evergreengazette.com",
        "image": "https://www.evergreengazette.com/evergreen-gazette-logo.png",
        "brand": {
            "@type": "brand",
            "name": "The Evergreen Gazette"
        },
        "offers": {
            "@type": "offer",
            "url": "https://subscribe.evergreengazette.com/acquisition?promo=h97"
        }
    },
    "publisher": {
        "@id": "evergreengazette.com",
        "@type": "NewsMediaOrganization",
        "name": "The Evergreen Gazette"
    },
    "isAccessibleForFree": false,
    "hasPart": {
        "@type": "WebPageElement",
        "cssSelector": ".paidContent",
        "isAccessibleForFree": false
    }
  }</script>
  <body>
    Content
    <div class="paidContent">Paid Content</div>
  </body>
  )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  // The root node contains paid content.
  EXPECT_TRUE(Content()->frame_data->contains_paid_content);

  const auto& root = ContentRootNode();

  // The text node should not have the paid content role.
  const auto& text_node = *root.children_nodes[0];
  EXPECT_FALSE(
      ContainsRole(text_node.content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));

  // The paid content node should have the paid content role.
  const auto& paid_node = *root.children_nodes[1];
  EXPECT_TRUE(
      ContainsRole(paid_node.content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
}

TEST_F(AIPageContentAgentTest, PaidContentContextMismatch) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <script type="application/ld+json">{
        "@context": "http://acme.org",
        "@type": "NewsArticle",
        "isAccessibleForFree": false,
        "hasPart": {
            "@type": "WebPageElement",
            "cssSelector": ".paidContent",
            "isAccessibleForFree": false
        }
      }</script>
      <body>
        Content
        <div class="paidContent">Paid Content</div>
      </body>
  )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  // The root node does not contain paid content.
  EXPECT_FALSE(Content()->frame_data->contains_paid_content);

  const auto& root = ContentRootNode();

  // The text node should not have the paid content role.
  const auto& text_node = *root.children_nodes[0];
  EXPECT_FALSE(
      ContainsRole(text_node.content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));

  // The paid content node should have the paid content role.
  const auto& paid_node = *root.children_nodes[1];
  EXPECT_FALSE(
      ContainsRole(paid_node.content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
}

TEST_F(AIPageContentAgentTest, PaidContentRootOnly) {
  // Note that isAccessibleForFree = "False" to match real world examples.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <script type="application/ld+json">{
        "@context": "http://schema.org",
        "@type": "NewsArticle",
        "isAccessibleForFree": "False",
        "hasPart": {
          "@type": "unrelated"
        }
      }</script>
      <body>
        Content
        <div class="paidContent">Paid Content</div>
      </body>
  )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  // The root node contains paid content.
  EXPECT_TRUE(Content()->frame_data->contains_paid_content);

  const auto& root = ContentRootNode();

  // The text node should not have the paid content role.
  const auto& text_node = *root.children_nodes[0];
  EXPECT_FALSE(
      ContainsRole(text_node.content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));

  // The paid content node should have the paid content role.
  const auto& paid_node = *root.children_nodes[1];
  EXPECT_FALSE(
      ContainsRole(paid_node.content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
}

TEST_F(AIPageContentAgentTest, PaidContentMicrodata) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <script type="application/ld+json">{
        "@context": "http://schema.org",
        "@type": "NewsArticle",
        "isAccessibleForFree": false
      }</script>
      <body>
        Content
        <div class="paidContent">
          <meta itemprop="isAccessibleForFree" content="false">
          Paid Content
        </div>
        <div class="paidContent">
          <meta itemprop="unrelated">
          Content
        </div>
      </body>
  )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  // The root node contains paid content.
  EXPECT_TRUE(Content()->frame_data->contains_paid_content);

  const auto& root = ContentRootNode();

  // The text node should not have the paid content role.
  const auto& text_node = *root.children_nodes[0];
  EXPECT_FALSE(
      ContainsRole(text_node.content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));

  // The paid content node should have the paid content role.
  const auto& paid_node = *root.children_nodes[1];
  EXPECT_TRUE(
      ContainsRole(paid_node.content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
}

TEST_F(AIPageContentAgentTest, PaidContentSomeYesSomeNo) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <script type="application/ld+json">{
        "@context": "http://schema.org",
        "@type": "NewsArticle",
        "isAccessibleForFree": false,
        "hasPart": {
            "@type": "WebPageElement",
            "cssSelector": ".paidContent",
            "isAccessibleForFree": false
        }
      }</script>
      <body>
        Content
        <div class="paidContent">Paid Content</div>
        <div>Free Content</div>
        <div class="paidContent">Paid Content</div>
        <div>Free Content</div>
      </body>
  )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  // The root node contains paid content.
  EXPECT_TRUE(Content()->frame_data->contains_paid_content);

  const auto& root = ContentRootNode();

  auto& nodes = root.children_nodes;

  // Every other node should have the paid content role.
  EXPECT_FALSE(
      ContainsRole(nodes[0]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_TRUE(
      ContainsRole(nodes[1]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_FALSE(
      ContainsRole(nodes[2]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_TRUE(
      ContainsRole(nodes[3]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_FALSE(
      ContainsRole(nodes[4]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
}

TEST_F(AIPageContentAgentTest, PaidContentMultipleHasParts) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <script type="application/ld+json">{
        "@context": "https://schema.org",
        "@type": "NewsArticle",
        "mainEntityOfPage": {
          "@type": "WebPage",
          "@id": "https://example.org/article"
          },
        "isAccessibleForFree": false,
        "hasPart": [
          {
            "@type": "WebPageElement",
            "isAccessibleForFree": false,
            "cssSelector": ".section1"
          }, {
            "@type": "WebPageElement",
            "isAccessibleForFree": false,
            "cssSelector": ".section2"
          }
        ]
      }</script>
      <body>
        Content
        <div class="section1">Paid Content</div>
        <div class="section2">Paid Content</div>
      </body>
  )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  // The root node contains paid content.
  EXPECT_TRUE(Content()->frame_data->contains_paid_content);

  const auto& root = ContentRootNode();

  auto& nodes = root.children_nodes;
  EXPECT_FALSE(
      ContainsRole(nodes[0]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_TRUE(
      ContainsRole(nodes[1]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_TRUE(
      ContainsRole(nodes[2]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
}

TEST_F(AIPageContentAgentTest, PaidContentSubframe) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <body>
      Parent doc is free!
      <iframe srcdoc='
        <script type="application/ld+json">{
          "@context": "http://schema.org",
          "@type": "NewsArticle",
          "isAccessibleForFree": false,
          "hasPart": {
              "@type": "WebPageElement",
              "cssSelector": ".paidContent",
              "isAccessibleForFree": false
          }
        }</script>
        <body>
          Content
          <div class="paidContent">Paid Content</div>
        </body>
      '></iframe>
      </body>
  )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  // The root node does not contain paid content.
  EXPECT_FALSE(Content()->frame_data->contains_paid_content);

  const auto& root = ContentRootNode();
  auto& nodes = root.children_nodes;

  EXPECT_FALSE(
      ContainsRole(nodes[0]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));

  const auto& iframe = nodes[1];
  EXPECT_EQ(iframe->content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);
  EXPECT_TRUE(iframe->content_attributes->iframe_data->local_frame_data
                  ->contains_paid_content);

  auto& children = iframe->children_nodes[0]->children_nodes;
  EXPECT_FALSE(
      ContainsRole(children[0]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_TRUE(
      ContainsRole(children[1]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
}

TEST_F(AIPageContentAgentTest, PaidContentSubframeMicrodata) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <script type="application/ld+json">{
        "@context": "https://schema.org",
        "@type": "NewsArticle",
        "isAccessibleForFree": true
      }</script>
      <body>
        Free Content
        <div class="paidContent">
          <meta itemprop="isAccessibleForFree" content="false">
          Microdata not checked
        </div>
        <iframe srcdoc='
          <script type="application/ld+json">{
            "@context": "http://schema.org",
            "@type": "NewsArticle",
            "isAccessibleForFree": false
          }</script>
          <body>
            Content
            <div class="paidContent">
              <meta itemprop="isAccessibleForFree" content="false">
              Paid Content
            </div>
          </body>
        '></iframe>
        <iframe srcdoc='
          <body>
            Content
            <div class="paidContent">
              <meta itemprop="isAccessibleForFree" content="false">
              Microdata not checked
            </div>
          </body>
        '></iframe>
        <iframe srcdoc='
          <script type="application/ld+json">{
            "@context": "http://schema.org",
            "@type": "NewsArticle",
            "isAccessibleForFree": false
          }</script>
          <body>
            Content
            <div class="paidContent">
              <meta itemprop="isAccessibleForFree" content="false">
              Paid Content
            </div>
          </body>
        '></iframe>
      </body>
  )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  // The root node does not contain paid content.
  EXPECT_FALSE(Content()->frame_data->contains_paid_content);

  const auto& root = ContentRootNode();
  auto& nodes = root.children_nodes;

  EXPECT_FALSE(
      ContainsRole(nodes[0]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_FALSE(
      ContainsRole(nodes[1]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));

  const auto& iframe1 = nodes[2];
  EXPECT_EQ(iframe1->content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);
  EXPECT_TRUE(iframe1->content_attributes->iframe_data->local_frame_data
                  ->contains_paid_content);

  const auto& children1 = iframe1->children_nodes[0]->children_nodes;
  EXPECT_FALSE(
      ContainsRole(children1[0]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_TRUE(
      ContainsRole(children1[1]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));

  const auto& iframe2 = nodes[3];
  EXPECT_EQ(iframe2->content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);
  EXPECT_FALSE(iframe2->content_attributes->iframe_data->local_frame_data
                   ->contains_paid_content);

  const auto& children2 = iframe2->children_nodes[0]->children_nodes;
  EXPECT_FALSE(
      ContainsRole(children2[0]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_FALSE(
      ContainsRole(children2[1]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));

  const auto& iframe3 = nodes[4];
  EXPECT_EQ(iframe3->content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);
  EXPECT_TRUE(iframe3->content_attributes->iframe_data->local_frame_data
                  ->contains_paid_content);

  const auto& children3 = iframe3->children_nodes[0]->children_nodes;
  EXPECT_FALSE(
      ContainsRole(children3[0]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_TRUE(
      ContainsRole(children3[1]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
}

TEST_F(AIPageContentAgentTest, HitTestElementsBasic) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <p style='background:red'>Text 1</p>"
      "  <p>Text 2</p>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  // The tree should look as follows, with the given z order.
  // root - 1
  // |_html - 2
  //    |_body - 3
  //      |_p - 4
  //      | |_Text1 - 6
  //      |_p - 5
  //        |_Text2 - 7
  const auto& root = *Content()->root_node;

  ASSERT_TRUE(root.content_attributes->node_interaction_info);
  EXPECT_EQ(
      root.content_attributes->node_interaction_info->document_scoped_z_order,
      1);

  ASSERT_EQ(root.children_nodes.size(), 1u);
  const auto& html = *root.children_nodes.at(0);
  EXPECT_EQ(
      html.content_attributes->node_interaction_info->document_scoped_z_order,
      2);

  ASSERT_EQ(html.children_nodes.size(), 1u);
  const auto& body = *html.children_nodes.at(0);
  EXPECT_EQ(
      body.content_attributes->node_interaction_info->document_scoped_z_order,
      3);

  ASSERT_EQ(body.children_nodes.size(), 2u);
  const auto& p1 = *body.children_nodes.at(0);
  EXPECT_EQ(
      p1.content_attributes->node_interaction_info->document_scoped_z_order, 4);

  const auto& p2 = *body.children_nodes.at(1);
  EXPECT_EQ(
      p2.content_attributes->node_interaction_info->document_scoped_z_order, 5);

  ASSERT_EQ(p1.children_nodes.size(), 1u);
  const auto& text1 = *p1.children_nodes.at(0);
  CheckTextNode(text1, "Text 1");
  EXPECT_EQ(
      text1.content_attributes->node_interaction_info->document_scoped_z_order,
      6);

  ASSERT_EQ(p2.children_nodes.size(), 1u);
  const auto& text2 = *p2.children_nodes.at(0);
  CheckTextNode(text2, "Text 2");
  EXPECT_EQ(
      text2.content_attributes->node_interaction_info->document_scoped_z_order,
      7);
}

TEST_F(AIPageContentAgentTest, HitTestElementsFixedPos) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <p style='position: fixed; top: 10px;'>Text 1</p>"
      "  <p>Text 2</p>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 2u);

  // The first node is now on top.
  const auto& p1 = *root.children_nodes.at(0);
  ASSERT_TRUE(p1.content_attributes->node_interaction_info);
  ASSERT_TRUE(
      p1.content_attributes->node_interaction_info->document_scoped_z_order);
  EXPECT_EQ(
      p1.content_attributes->node_interaction_info->document_scoped_z_order, 6);

  const auto& p2 = *root.children_nodes.at(1);
  ASSERT_TRUE(p2.content_attributes->node_interaction_info);
  ASSERT_TRUE(
      p2.content_attributes->node_interaction_info->document_scoped_z_order);
  EXPECT_EQ(
      p2.content_attributes->node_interaction_info->document_scoped_z_order, 4);
}

TEST_F(AIPageContentAgentTest, HitTestElementsPointerNone) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <p style='pointer-events:none'>Text 1</p>"
      "  <p>Text 2</p>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 2u);

  // The first node is not actionable anymore.
  const auto& p1 = *root.children_nodes.at(0);
  EXPECT_FALSE(p1.content_attributes->node_interaction_info);

  const auto& p2 = *root.children_nodes.at(1);
  ASSERT_TRUE(p2.content_attributes->node_interaction_info);
  ASSERT_TRUE(
      p2.content_attributes->node_interaction_info->document_scoped_z_order);
}

TEST_F(AIPageContentAgentTest, HitTestElementsOffscreen) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <p style='cursor:pointer; position:fixed; top:110vh;'>Text 1</p>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);

  // The first node is actionable but not in viewport
  const auto& p1 = *root.children_nodes.at(0);
  ASSERT_TRUE(p1.content_attributes->node_interaction_info);
  const auto& interaction_info = *p1.content_attributes->node_interaction_info;
  EXPECT_TRUE(interaction_info.is_clickable);
  EXPECT_FALSE(interaction_info.document_scoped_z_order);
}

TEST_F(AIPageContentAgentTest, HitTestElementsIframe) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body>
        <iframe srcdoc='<p>Text 1</p>'></iframe>
        <p>Text 2</p>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  // The iframe and outer p have z order relative to each other.
  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 2u);

  const auto& iframe = *root.children_nodes.at(0);
  ASSERT_TRUE(iframe.content_attributes->node_interaction_info);
  ASSERT_TRUE(iframe.content_attributes->node_interaction_info
                  ->document_scoped_z_order);

  const auto& p = *root.children_nodes.at(1);
  ASSERT_TRUE(p.content_attributes->node_interaction_info);
  ASSERT_TRUE(
      p.content_attributes->node_interaction_info->document_scoped_z_order);

  EXPECT_GT(
      *iframe.content_attributes->node_interaction_info
           ->document_scoped_z_order,
      *p.content_attributes->node_interaction_info->document_scoped_z_order);

  ASSERT_EQ(iframe.children_nodes.size(), 1u);
  const auto& doc_inside_iframe = *iframe.children_nodes.at(0);
  ASSERT_TRUE(doc_inside_iframe.content_attributes->node_interaction_info);
  ASSERT_TRUE(doc_inside_iframe.content_attributes->node_interaction_info
                  ->document_scoped_z_order);
  EXPECT_EQ(*doc_inside_iframe.content_attributes->node_interaction_info
                 ->document_scoped_z_order,
            1);
}

TEST_F(AIPageContentAgentTest, OverflowHiddenGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <div style='width: 100px; height: 100px; overflow-y: hidden;'>"
      "     <article style='width: 50px; height: 300px;'></article>"
      "   </div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& outer = ContentRootNode().children_nodes[0];
  const auto& article = outer->children_nodes[0];
  CheckAnnotatedRole(*article,
                     mojom::blink::AIPageContentAnnotatedRole::kArticle);

  EXPECT_GT(*article->content_attributes->node_interaction_info
                 ->document_scoped_z_order,
            *outer->content_attributes->node_interaction_info
                 ->document_scoped_z_order);

  CheckGeometry(*outer, gfx::Rect(8, 8, 100, 100), gfx::Rect(8, 8, 100, 100));
  CheckGeometry(*article, gfx::Rect(8, 8, 50, 300), gfx::Rect(8, 8, 50, 100));
}

TEST_F(AIPageContentAgentTest, OverflowVisibleGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <section style='width: 100px; height: 100px; overflow-y: visible;'>"
      "    <article style='width: 50px; height: 300px;'></article>"
      "  </section>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& outer = ContentRootNode().children_nodes[0];
  const auto& article = outer->children_nodes[0];
  CheckAnnotatedRole(*article,
                     mojom::blink::AIPageContentAnnotatedRole::kArticle);

  EXPECT_GT(*article->content_attributes->node_interaction_info
                 ->document_scoped_z_order,
            *outer->content_attributes->node_interaction_info
                 ->document_scoped_z_order);

  CheckGeometry(*outer, gfx::Rect(8, 8, 100, 100), gfx::Rect(8, 8, 100, 100));
  CheckGeometry(*article, gfx::Rect(8, 8, 50, 300), gfx::Rect(8, 8, 50, 300));
}

TEST_F(AIPageContentAgentTest, BlurGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <section style='width: 100px; height: 100px; filter: "
      "blur(10px);'></section>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& section = Content()->root_node->children_nodes[0];
  CheckAnnotatedRole(*section,
                     mojom::blink::AIPageContentAnnotatedRole::kSection);

  CheckGeometry(*section, gfx::Rect(8, 8, 100, 100), gfx::Rect(8, 8, 100, 100));
}

TEST_F(AIPageContentAgentTest, GeomtryAbsPos) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <section style='width: 100px; height: 100px; position: absolute; top: "
      "200px; left: 200px;'>"
      "</section>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& section = Content()->root_node->children_nodes[0];
  CheckAnnotatedRole(*section,
                     mojom::blink::AIPageContentAnnotatedRole::kSection);

  CheckGeometry(*section, gfx::Rect(200, 200, 100, 100),
                gfx::Rect(200, 200, 100, 100));
}

TEST_F(AIPageContentAgentTest, HitTestElementsRelativePos) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <section style='width: 100px; height: 100px; position: relative; "
      "overflow: clip;'>"
      "    <article style='width: 50px; height: 50px; position: absolute; "
      "left: "
      "150px; top:0px;'></article>"
      "  </section>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& outer = ContentRootNode().children_nodes[0];
  const auto& article = outer->children_nodes[0];
  CheckAnnotatedRole(*article,
                     mojom::blink::AIPageContentAnnotatedRole::kArticle);

  EXPECT_GT(*article->content_attributes->node_interaction_info
                 ->document_scoped_z_order,
            *outer->content_attributes->node_interaction_info
                 ->document_scoped_z_order);

  CheckGeometry(*outer, gfx::Rect(8, 8, 100, 100), gfx::Rect(8, 8, 100, 100));
  CheckGeometry(*article, gfx::Rect(158, 8, 50, 50), gfx::Rect());
}

TEST_F(AIPageContentAgentTest, GeometryTransform) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <section style='width: 100px; height: 100px; transform: "
      "translate(200px, 200px)'>"
      "</section>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& section = ContentRootNode().children_nodes[0];
  CheckAnnotatedRole(*section,
                     mojom::blink::AIPageContentAnnotatedRole::kSection);

  CheckGeometry(*section, gfx::Rect(208, 208, 100, 100),
                gfx::Rect(208, 208, 100, 100));
}

TEST_F(AIPageContentAgentTest, CursorForClickability) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <div style='cursor: pointer'>"
      "    <p>no-click</p>"
      "    <p style='cursor: pointer'>click</p>"
      "  </div>"
      "  <article>article</article>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  EXPECT_EQ(ContentRootNode().children_nodes.size(), 2u);

  const auto& cursor = *ContentRootNode().children_nodes[0];
  EXPECT_TRUE(cursor.content_attributes->node_interaction_info);
  EXPECT_TRUE(cursor.content_attributes->node_interaction_info->is_clickable);

  const auto& no_click = *cursor.children_nodes[0];
  EXPECT_TRUE(no_click.content_attributes->node_interaction_info);
  EXPECT_FALSE(
      no_click.content_attributes->node_interaction_info->is_clickable);

  const auto& click = *cursor.children_nodes[1];
  EXPECT_TRUE(click.content_attributes->node_interaction_info);
  EXPECT_TRUE(click.content_attributes->node_interaction_info->is_clickable);

  const auto& article = *ContentRootNode().children_nodes[1];
  EXPECT_TRUE(article.content_attributes->node_interaction_info);
  EXPECT_FALSE(article.content_attributes->node_interaction_info->is_clickable);
}

TEST_F(AIPageContentAgentTest, LinkForClickability) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <a href='test.com'>valid</a>"
      "  <a>invalid</a>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  EXPECT_EQ(ContentRootNode().children_nodes.size(), 2u);

  const auto& valid = *ContentRootNode().children_nodes[0];
  EXPECT_TRUE(valid.content_attributes->node_interaction_info);
  EXPECT_TRUE(valid.content_attributes->node_interaction_info->is_clickable);

  const auto& invalid = *ContentRootNode().children_nodes[1];
  EXPECT_TRUE(invalid.content_attributes->node_interaction_info);
  EXPECT_FALSE(invalid.content_attributes->node_interaction_info->is_clickable);
}

TEST_F(AIPageContentAgentTest, LabelWithForSibling) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      " <input type='checkbox' id='myCheckbox' />"
      " <label for='myCheckbox'>Check me!</label>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 2u);

  const auto& input = *root.children_nodes[0];
  CheckFormControlNode(input, mojom::blink::FormControlType::kInputCheckbox);
  ASSERT_TRUE(input.content_attributes->node_interaction_info);
  EXPECT_TRUE(input.content_attributes->node_interaction_info->is_clickable);

  const auto& label = *root.children_nodes[1];
  CheckContainerNode(label);
  ASSERT_TRUE(label.content_attributes->node_interaction_info);
  EXPECT_TRUE(label.content_attributes->node_interaction_info->is_clickable);
  EXPECT_EQ(label.content_attributes->label_for_dom_node_id,
            input.content_attributes->dom_node_id);
}

TEST_F(AIPageContentAgentTest, LabelWithForDescendant) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      " <label>"
      "   <input type='checkbox' id='myCheckbox' />"
      "Check me!"
      "</label>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& label = *root.children_nodes[0];
  EXPECT_EQ(label.children_nodes.size(), 2u);
  CheckContainerNode(label);
  ASSERT_TRUE(label.content_attributes->node_interaction_info);
  EXPECT_TRUE(label.content_attributes->node_interaction_info->is_clickable);

  const auto& input = *label.children_nodes[0];
  CheckFormControlNode(input, mojom::blink::FormControlType::kInputCheckbox);
  ASSERT_TRUE(input.content_attributes->node_interaction_info);
  EXPECT_TRUE(input.content_attributes->node_interaction_info->is_clickable);
  EXPECT_EQ(label.content_attributes->label_for_dom_node_id,
            input.content_attributes->dom_node_id);

  CheckTextNode(*label.children_nodes[1], "Check me!");
}

TEST_F(AIPageContentAgentTest, SVG) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <svg width='400' height='200'>"
      "    <text x='50%' y='50/%' font-size='24'>"
      "      Hello SVG Text!"
      "    </text>"
      "  </svg>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& svg = *ContentRootNode().children_nodes[0];
  EXPECT_EQ(svg.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kSVG);
  ASSERT_TRUE(svg.content_attributes->svg_data);
  EXPECT_EQ(svg.content_attributes->svg_data->inner_text, "Hello SVG Text!");
}

TEST_F(AIPageContentAgentTest, SVGWithNoText) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <svg width='400' height='200' style='content-visibility: hidden'>"
      "    <text x='50%' y='50/%' font-size='24'>"
      "      Hello SVG Text!"
      "    </text>"
      "  </svg>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& svg = *ContentRootNode().children_nodes[0];
  EXPECT_EQ(svg.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kSVG);
  ASSERT_TRUE(svg.content_attributes->svg_data);
  EXPECT_FALSE(svg.content_attributes->svg_data->inner_text);
}

TEST_F(AIPageContentAgentTest, Canvas) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    canvas {"
      "      width: 200px;"
      "      height: 300px;"
      "    }"
      "  </style>"
      "  <canvas id='myCanvas' width='100' height='200'></canvas>"
      "  <script>"
      "    const canvas = document.getElementById('myCanvas');"
      "    const ctx = canvas.getContext('2d');"
      "    ctx.fillStyle = 'pink';"
      "    ctx.fillRect(0, 0, 100, 200);"
      "  </script>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& canvas = *ContentRootNode().children_nodes[0];
  EXPECT_EQ(canvas.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kCanvas);
  ASSERT_TRUE(canvas.content_attributes->canvas_data);
  EXPECT_EQ(canvas.content_attributes->canvas_data->layout_size,
            gfx::Size(200, 300));
}

TEST_F(AIPageContentAgentTest, AriaLabelledBy) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      " <div id='hiddenLabel1' style='display: none;'>and first</div>"
      " <div id='hiddenLabel2' style='display: none;'>and second</div>"
      " <input type='text' aria-labelledby='hiddenLabel1 hiddenLabel2' "
      "aria-label='on element'/>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& input = *root.children_nodes[0];
  CheckFormControlNode(input, mojom::blink::FormControlType::kInputText);
  EXPECT_EQ(input.content_attributes->label, "on element and first and second");
}

TEST_F(AIPageContentAgentTest, DisabledButton) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
        <body>
         <button disabled>Text</button>
        </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);
  const auto& button = *root.children_nodes.at(0);
  CheckHitTestableButNotInteractive(button);
}

TEST_F(AIPageContentAgentTest, ActionablePseudoElements) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style> a::before { content: 'hello'; cursor: pointer;} </style>"
      "  <a href='#'></a>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  EXPECT_EQ(ContentRootNode().children_nodes.size(), 1u);
  const auto& a = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(a.content_attributes->node_interaction_info);
  EXPECT_TRUE(a.content_attributes->node_interaction_info->is_clickable);
  EXPECT_TRUE(
      a.content_attributes->node_interaction_info->document_scoped_z_order);

  EXPECT_EQ(a.children_nodes.size(), 1u);
  const auto& before = *a.children_nodes[0];
  ASSERT_TRUE(before.content_attributes->node_interaction_info);
  EXPECT_TRUE(before.content_attributes->node_interaction_info->is_clickable);
  EXPECT_TRUE(before.content_attributes->node_interaction_info
                  ->document_scoped_z_order);
}

TEST_F(AIPageContentAgentTest, PseudoElementNotActionable) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style> a::before { content: 'hello';} </style>"
      "  <a href='#'></a>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  EXPECT_EQ(ContentRootNode().children_nodes.size(), 1u);

  const auto& a = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(a.content_attributes->node_interaction_info);
  EXPECT_TRUE(a.content_attributes->node_interaction_info->is_clickable);
  EXPECT_TRUE(
      a.content_attributes->node_interaction_info->document_scoped_z_order);

  EXPECT_EQ(a.children_nodes.size(), 1u);
  const auto& before = *a.children_nodes[0];
  ASSERT_TRUE(before.content_attributes->node_interaction_info);
  EXPECT_FALSE(before.content_attributes->node_interaction_info->is_clickable);
  EXPECT_TRUE(before.content_attributes->node_interaction_info
                  ->document_scoped_z_order);
}

TEST_F(AIPageContentAgentTest, PseudoElementNoPointerEvents) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style> a::before { content: 'hello'; pointer-events: none;} </style>"
      "  <a href='#'></a>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  EXPECT_EQ(Content()->root_node->children_nodes.size(), 1u);

  const auto& a = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(a.content_attributes->node_interaction_info);
  EXPECT_TRUE(a.content_attributes->node_interaction_info->is_clickable);
  EXPECT_TRUE(
      a.content_attributes->node_interaction_info->document_scoped_z_order);

  EXPECT_EQ(a.children_nodes.size(), 1u);
  const auto& text = *a.children_nodes[0];
  CheckTextNode(text, "hello");
}

TEST_F(AIPageContentAgentTest, AriaDisabled) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body>
        <section style='cursor: pointer' aria-disabled=true>
          <input type=text aria-disabled=false></input>
        </section>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);

  // The first node is not actionable anymore.
  const auto& section = *root.children_nodes.at(0);
  CheckContainerNode(section);
  CheckHitTestableButNotInteractive(section);

  // The child is also not actionable.
  ASSERT_EQ(section.children_nodes.size(), 1u);
  const auto& input = *section.children_nodes.at(0);
  CheckHitTestableButNotInteractive(input);
}

TEST_F(AIPageContentAgentTest, DisabledInheritance) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body>
        <form>
          <fieldset disabled>
            <button type="submit"></button>
          </fieldset>
        </form>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& form = *root.children_nodes.at(0);
  CheckHitTestableButNotInteractive(form);
  ASSERT_EQ(form.children_nodes.size(), 1u);

  const auto& fieldset = *form.children_nodes.at(0);
  CheckHitTestableButNotInteractive(fieldset);
  ASSERT_EQ(fieldset.children_nodes.size(), 1u);

  const auto& button = *fieldset.children_nodes.at(0);
  CheckHitTestableButNotInteractive(button);
}

TEST_F(AIPageContentAgentTest, DisabledOption) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body>
        <select>
          <option value="banana">Banana</option>
          <option value="cherry" disabled>Cherry</option>
        </select>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& select = *root.children_nodes.at(0);
  CheckFormControlNode(select, mojom::blink::FormControlType::kSelectOne);

  const auto& options =
      select.content_attributes->form_control_data->select_options;
  ASSERT_EQ(options.size(), 2u);

  const auto& banana = *options.at(0);
  EXPECT_EQ(banana.value, "banana");
  EXPECT_FALSE(banana.disabled);

  const auto& cherry = *options.at(1);
  EXPECT_EQ(cherry.value, "cherry");
  EXPECT_TRUE(cherry.disabled);
}

TEST_F(AIPageContentAgentTest, AriaRole) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body>
        <div role="button"></div>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& button = *root.children_nodes.at(0);
  ASSERT_TRUE(button.content_attributes->node_interaction_info);
  EXPECT_TRUE(button.content_attributes->node_interaction_info->is_clickable);
  EXPECT_EQ(button.content_attributes->aria_role,
            ax::mojom::blink::Role::kButton);
}

TEST_F(AIPageContentAgentTest, LabelNotActionable) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
        <body>
          <input type='checkbox' id='myCheckbox' />
          <label for='myCheckbox' style='pointer-events: none;'>Check me!</label>
        </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 2u);

  const auto& button = *root.children_nodes.at(0);
  ASSERT_TRUE(button.content_attributes->node_interaction_info);
  EXPECT_TRUE(button.content_attributes->node_interaction_info->is_clickable);

  const auto& label = *root.children_nodes.at(1);
  EXPECT_FALSE(label.content_attributes->node_interaction_info);
  EXPECT_EQ(*label.content_attributes->label_for_dom_node_id,
            button.content_attributes->dom_node_id);
}

TEST_F(AIPageContentAgentTest, SelectLabelNotActionable) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
        <body>
          <label for="fruit-select">Choose a fruit:</label>
          <select id="fruit-select" name="fruits">
            <option value="">--Please choose an option--</option>
            <option value="apple">Apple</option>
            <option value="banana">Banana</option>
          </select>
        </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 2u);

  const auto& label = *root.children_nodes.at(0);
  ASSERT_TRUE(label.content_attributes->node_interaction_info);
  EXPECT_FALSE(label.content_attributes->node_interaction_info->is_clickable);

  const auto& select = *root.children_nodes.at(1);
  ASSERT_TRUE(select.content_attributes->node_interaction_info);
  EXPECT_TRUE(select.content_attributes->node_interaction_info->is_clickable);
}

}  // namespace
}  // namespace blink
