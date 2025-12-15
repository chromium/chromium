// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include <cstddef>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/test/with_feature_override.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_ad_evidence.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-data-view.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/graphics/visual_rect_flags.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
using ClickabilityReason = mojom::blink::AIPageContentClickabilityReason;
using InteractionDisabledReason =
    mojom::blink::AIPageContentInteractionDisabledReason;

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
    helper_.LoadAhem();
    ASSERT_TRUE(helper_.LocalMainFrame());
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void LoadAhem() {
    helper_.LoadAhem();
    test::RunPendingTasks();
    helper_.LocalMainFrame()
        ->GetFrame()
        ->GetDocument()
        ->View()
        ->UpdateAllLifecyclePhasesForTest();
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
    options.mode = mojom::blink::AIPageContentMode::kActionableElements;
    GetAIPageContent(options);
  }

  static mojom::blink::AIPageContentOptions GetAIPageContentOptionsForTest() {
    mojom::blink::AIPageContentOptions options;
    options.on_critical_path = true;
    options.mode = mojom::blink::AIPageContentMode::kDefault;
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
    if (last_options_.mode !=
        mojom::blink::AIPageContentMode::kActionableElements) {
      return *last_content_->root_node;
    }

    EXPECT_EQ(last_content_->root_node->children_nodes.size(), 1u);
    const auto& html = *last_content_->root_node->children_nodes[0];

    EXPECT_EQ(html.children_nodes.size(), 1u);
    return *html.children_nodes[0];
  }

  const mojom::blink::AIPageContentNode* FindNodeByDomNodeId(
      DOMNodeId dom_node_id) {
    const auto& root = ContentRootNode();
    Vector<const mojom::blink::AIPageContentNode*> stack;
    stack.push_back(&root);
    while (!stack.empty()) {
      const auto* node = stack.back();
      stack.pop_back();
      if (node->content_attributes &&
          node->content_attributes->dom_node_id.has_value() &&
          *node->content_attributes->dom_node_id == dom_node_id) {
        return node;
      }
      for (size_t i = node->children_nodes.size(); i > 0; --i) {
        stack.push_back(node->children_nodes[i - 1].get());
      }
    }
    return nullptr;
  }

  const mojom::blink::AIPageContentNode* FindNodeBySelector(String selector) {
    Document* document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
    EXPECT_TRUE(document);
    Element* element = document->QuerySelector(AtomicString(selector));
    EXPECT_TRUE(element) << "Couldn't find element with selector = "
                         << selector;
    DOMNodeId dom_node_id = DOMNodeIds::IdForNode(element);
    EXPECT_GE(dom_node_id, 1);
    return FindNodeByDomNodeId(dom_node_id);
  }

  void CheckHitTestableButNotInteractive(
      const mojom::blink::AIPageContentNode& node) {
    CHECK(node.content_attributes->node_interaction_info);
    EXPECT_TRUE(node.content_attributes->node_interaction_info
                    ->document_scoped_z_order);
    EXPECT_TRUE(node.content_attributes->node_interaction_info
                    ->clickability_reasons.empty());
  }

  void CheckHitTestableAndInteractive(
      const mojom::blink::AIPageContentNode& node,
      base::span<const ClickabilityReason> expected_reasons) {
    CHECK(node.content_attributes->node_interaction_info);
    EXPECT_TRUE(node.content_attributes->node_interaction_info
                    ->document_scoped_z_order);
    EXPECT_THAT(
        node.content_attributes->node_interaction_info->clickability_reasons,
        testing::UnorderedElementsAreArray(expected_reasons));
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

  GetAIPageContentWithActionableElements();

  const auto& root = *Content()->root_node;
  EXPECT_EQ(root.children_nodes.size(), 1u);

  const auto& attributes = *root.content_attributes;
  EXPECT_TRUE(attributes.dom_node_id.has_value());

  EXPECT_EQ(attributes.attribute_type,
            mojom::blink::AIPageContentAttributeType::kRoot);

  CheckGeometry(root, gfx::Rect(kWindowSize), gfx::Rect(kWindowSize));

  const auto& text_node = *ContentRootNode().children_nodes[0];
  CheckTextNode(text_node, "text");

  const auto& text_attributes = *text_node.content_attributes;
  ASSERT_TRUE(text_attributes.geometry);
  EXPECT_FALSE(text_attributes.node_interaction_info);
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

  GetAIPageContentWithActionableElements();

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

TEST_F(AIPageContentAgentTest, ImageIsAdRelated) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <img id='ads'></img>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  auto& document = *helper_.LocalMainFrame()->GetFrame()->GetDocument();
  To<HTMLImageElement>(document.getElementById(AtomicString("ads")))
      ->SetIsAdRelated();

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  auto& image_node = *root.children_nodes[0];
  EXPECT_TRUE(image_node.content_attributes->is_ad_related);
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

TEST_F(AIPageContentAgentTest, Video) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <video src='https://example.com/video.mp4'></video>"
      "  <video "
      "src='https://example.com/video.mp4?param1=value1&param2=value2'></video>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 2u);

  const auto& video1 = *root.children_nodes[0];
  EXPECT_EQ(video1.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kVideo);
  ASSERT_TRUE(video1.content_attributes->video_data);
  EXPECT_EQ(video1.content_attributes->video_data->url,
            blink::KURL("https://example.com/video.mp4"));

  const auto& video2 = *root.children_nodes[1];
  EXPECT_EQ(video2.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kVideo);
  ASSERT_TRUE(video2.content_attributes->video_data);
  EXPECT_EQ(
      video2.content_attributes->video_data->url,
      blink::KURL("https://example.com/video.mp4?param1=value1&param2=value2"));
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

  GetAIPageContentWithActionableElements();

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

  iframe_doc->body()->SetInnerHTMLWithoutTrustedTypes(
      "<body>inside iframe</body>");

  GetAIPageContent();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& iframe = *root.children_nodes[0];
  const auto& iframe_attributes = *iframe.content_attributes;

  EXPECT_EQ(iframe_attributes.attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);
  EXPECT_FALSE(iframe_attributes.is_ad_related);

  const auto& iframe_root = *iframe.children_nodes[0];
  CheckTextNode(*iframe_root.children_nodes[0], "inside iframe");
}

TEST_F(AIPageContentAgentTest, IFrameAds) {
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

  // Mark iframe's ad evidence.
  blink::FrameAdEvidence ad_evidence;
  ad_evidence.set_created_by_ad_script(
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
  ad_evidence.set_is_complete();
  To<LocalFrame>(iframe_element->ContentFrame())->SetAdEvidence(ad_evidence);

  GetAIPageContent();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& iframe = *root.children_nodes[0];
  const auto& iframe_attributes = *iframe.content_attributes;

  EXPECT_EQ(iframe_attributes.attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);
  EXPECT_TRUE(iframe_attributes.is_ad_related);
}

TEST_F(AIPageContentAgentTest, CrossSiteIframeIncluded) {
  KURL main_url = url_test_helpers::ToKURL("http://example.com/main.html");
  KURL cross_origin_url =
      url_test_helpers::ToKURL("http://www.example.com/frame.html");
  KURL cross_site_url =
      url_test_helpers::ToKURL("http://altostrat.com/frame_another.html");

  // Mock the cross origin, same-site iframe's content.
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8("http://www.example.com/"), test::CoreTestDataPath(),
      WebString::FromUTF8("frame.html"));

  // Mock the cross-site iframe's content.
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8("http://altostrat.com/"), test::CoreTestDataPath(),
      WebString::FromUTF8("frame_another.html"));

  // Load the main page which contains the same-site iframe and the cross-origin
  // iframe.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "<iframe src='http://www.example.com/frame.html'></iframe>"
      "<iframe src='http://altostrat.com/frame_another.html'></iframe>"
      "</body>",
      main_url);

  // Let the iframe load.
  test::RunPendingTasks();

  auto options = GetAIPageContentOptionsForTest();

  options.include_same_site_only = false;
  GetAIPageContent(options);

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 2u);

  // Both nodes should be present.
  const auto& same_site_iframe_node = *root.children_nodes[0];
  const auto& cross_site_iframe_node = *root.children_nodes[1];
  CheckIframeNode(same_site_iframe_node);
  CheckIframeNode(cross_site_iframe_node);

  // The contents of both nodes should be present as well.
  ASSERT_EQ(same_site_iframe_node.children_nodes.size(), 1u);
  ASSERT_EQ(cross_site_iframe_node.children_nodes.size(), 1u);

  const auto& same_site_iframe_root = *same_site_iframe_node.children_nodes[0];
  const auto& cross_site_iframe_root =
      *cross_site_iframe_node.children_nodes[0];

  CheckRootNode(same_site_iframe_root);
  CheckRootNode(cross_site_iframe_root);
  ASSERT_EQ(same_site_iframe_root.children_nodes.size(), 1u);
  ASSERT_EQ(cross_site_iframe_root.children_nodes.size(), 1u);

  CheckTextNode(*same_site_iframe_root.children_nodes[0], "I am an iframe\n");
  CheckTextNode(*cross_site_iframe_root.children_nodes[0],
                "I am another iframe\n");
}

TEST_F(AIPageContentAgentTest, CrossSiteIframeExcluded) {
  KURL main_url = url_test_helpers::ToKURL("http://example.com/main.html");
  KURL cross_origin_url =
      url_test_helpers::ToKURL("http://www.example.com/frame.html");
  KURL cross_site_url =
      url_test_helpers::ToKURL("http://altostrat.com/frame_another.html");

  // Mock the cross origin, same-site iframe's content.
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8("http://www.example.com/"), test::CoreTestDataPath(),
      WebString::FromUTF8("frame.html"));

  // Mock the cross-site iframe's content.
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8("http://altostrat.com/"), test::CoreTestDataPath(),
      WebString::FromUTF8("frame_another.html"));

  // Load the main page which contains the same-site iframe and the cross-origin
  // iframe.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "<iframe src='http://www.example.com/frame.html'></iframe>"
      "<iframe src='http://altostrat.com/frame_another.html'></iframe>"
      "</body>",
      main_url);

  // Let the iframe load.
  test::RunPendingTasks();

  auto options = GetAIPageContentOptionsForTest();

  options.include_same_site_only = true;
  GetAIPageContent(options);

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 2u);

  // Both nodes should be present.
  const auto& same_site_iframe_node = *root.children_nodes[0];
  const auto& cross_site_iframe_node = *root.children_nodes[1];
  CheckIframeNode(same_site_iframe_node);
  CheckIframeNode(cross_site_iframe_node);

  // Only the contents of the same-site iframe should be present.
  ASSERT_EQ(same_site_iframe_node.children_nodes.size(), 1u);
  ASSERT_TRUE(cross_site_iframe_node.children_nodes.empty());
  ASSERT_EQ(cross_site_iframe_node.content_attributes->iframe_data->content
                ->get_redacted_frame_metadata()
                ->reason,
            blink::mojom::RedactedFrameMetadata_Reason::kCrossSite);

  const auto& same_site_iframe_root = *same_site_iframe_node.children_nodes[0];

  CheckRootNode(same_site_iframe_root);
  ASSERT_EQ(same_site_iframe_root.children_nodes.size(), 1u);

  CheckTextNode(*same_site_iframe_root.children_nodes[0], "I am an iframe\n");
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
  ASSERT_EQ(table.children_nodes.size(), 5u);

  const auto& caption_text = *table.children_nodes[0];
  CheckTextNode(caption_text, "Table caption");

  const auto& header1 = *table.children_nodes[1];
  CheckTableRowNode(header1, mojom::blink::AIPageContentTableRowType::kHeader);
  ASSERT_EQ(header1.children_nodes.size(), 1u);

  const auto& header1_cell1 = *header1.children_nodes[0];
  CheckTableCellNode(header1_cell1);
  CheckTextNode(*header1_cell1.children_nodes[0], "Header");

  const auto& row1 = *table.children_nodes[2];
  CheckTableRowNode(row1, mojom::blink::AIPageContentTableRowType::kBody);
  ASSERT_EQ(row1.children_nodes.size(), 2u);

  const auto& row1_cell1 = *row1.children_nodes[0];
  CheckTableCellNode(row1_cell1);
  CheckTextNode(*row1_cell1.children_nodes[0], "Row 1 Column 1");

  const auto& row1_cell2 = *row1.children_nodes[1];
  CheckTableCellNode(row1_cell2);
  CheckTextNode(*row1_cell2.children_nodes[0], "Row 1 Column 2");

  const auto& row2 = *table.children_nodes[3];
  CheckTableRowNode(row2, mojom::blink::AIPageContentTableRowType::kBody);
  ASSERT_EQ(row2.children_nodes.size(), 2u);

  const auto& row2_cell1 = *row2.children_nodes[0];
  CheckTableCellNode(row2_cell1);
  CheckTextNode(*row2_cell1.children_nodes[0], "Row 2 Column 1");

  const auto& row2_cell2 = *row2.children_nodes[1];
  CheckTableCellNode(row2_cell2);
  CheckTextNode(*row2_cell2.children_nodes[0], "Row 2 Column 2");

  const auto& footer = *table.children_nodes[4];
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

TEST_F(AIPageContentAgentTest, FigureCaptionDisplayAsTableCaption) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
        <body>
          <figure style='display:table'>
            <figcaption style='display:table-caption'>
              <a href='https://www.youtube.com/'>Youtube</a>
            </figcaption>
          </figure>
        </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);
  const auto& figure = *root.children_nodes.at(0);
  ASSERT_TRUE(figure.content_attributes->node_interaction_info);
  EXPECT_EQ(figure.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kTable);

  ASSERT_EQ(figure.children_nodes.size(), 1u);
  const auto& fig_caption = *figure.children_nodes.at(0);
  ASSERT_TRUE(fig_caption.content_attributes->node_interaction_info);
  EXPECT_EQ(fig_caption.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kContainer);

  ASSERT_EQ(fig_caption.children_nodes.size(), 1u);
  const auto& anchor = *fig_caption.children_nodes.at(0);
  CheckAnchorNode(anchor, blink::KURL("https://www.youtube.com/"), {});
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

  const auto& hidden_text_node = *hidden_container.children_nodes[0];
  CheckTextNode(hidden_text_node, "hidden text");

  const auto& visible_text_node = *root.children_nodes[1];
  CheckTextNode(visible_text_node, "visible text");
}

TEST_F(AIPageContentAgentTest, HiddenUntilFoundNoInvalidationAllowed) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body>
        <style>
          body {
            margin: 0; font-size: 100px;
          }
        </style>
        <header hidden=until-found>hidden text</header><div>visible text</div>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  LocalFrameView::InvalidationDisallowedScope disallow(
      *helper_.LocalMainFrame()->GetFrame()->View());
  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 2u);

  const auto& hidden_container = *root.children_nodes[0];
  CheckContainerNode(hidden_container);
  CheckAnnotatedRoles(
      hidden_container,
      {mojom::blink::AIPageContentAnnotatedRole::kHeader,
       mojom::blink::AIPageContentAnnotatedRole::kContentHidden});
  EXPECT_TRUE(hidden_container.children_nodes.empty());

  const auto& visible_text_node = *root.children_nodes[1];
  CheckTextNode(visible_text_node, "visible text");
}

TEST_F(AIPageContentAgentTest, HiddenUntilFoundGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    body {"
      "      margin: 0; font-size: 100px;"
      "    }"
      "  </style>"
      "  <header hidden=until-found>hidden text</header>visible text"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

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

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_FALSE(root.content_attributes->geometry);

  EXPECT_EQ(root.children_nodes.size(), 1u);
  const auto& text_node = *root.children_nodes[0];
  CheckTextNode(text_node, "text");
  EXPECT_FALSE(text_node.content_attributes->geometry);
}

TEST_F(AIPageContentAgentTest, FormWithTextInput) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <form name='myform' action='https://example.com/submit'>"
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
  // The form metadata must expose the normalized action URL so callers can
  // surface the submit destination.
  EXPECT_EQ(form.content_attributes->form_data->action_url,
            url_test_helpers::ToKURL("https://example.com/submit"));
  EXPECT_EQ(form.children_nodes.size(), 4u);

  CheckTextNode(*form.children_nodes[0], "Lorem Ipsum");

  const auto& text_input1 = *form.children_nodes[1];
  CheckFormControlNode(text_input1, mojom::blink::FormControlType::kInputText);
  EXPECT_EQ(text_input1.content_attributes->form_control_data->field_name,
            "LI");
  EXPECT_EQ(text_input1.content_attributes->form_control_data->field_value,
            "Lorem");
  EXPECT_EQ(
      text_input1.content_attributes->form_control_data->redaction_decision,
      mojom::AIPageContentRedactionDecision::kNoRedactionNecessary);
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
  EXPECT_EQ(
      text_input2.content_attributes->form_control_data->redaction_decision,
      mojom::AIPageContentRedactionDecision::kNoRedactionNecessary);
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
  EXPECT_EQ(select.content_attributes->form_control_data->redaction_decision,
            mojom::AIPageContentRedactionDecision::kNoRedactionNecessary);

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
  EXPECT_EQ(radio1.content_attributes->form_control_data->redaction_decision,
            mojom::AIPageContentRedactionDecision::kNoRedactionNecessary);
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
      "    <input id='pwd' type='password' name='Enter password' "
      "value='mypassword'>"
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
  EXPECT_EQ(password.content_attributes->form_control_data->redaction_decision,
            mojom::AIPageContentRedactionDecision::kRedacted_HasBeenPassword);
  EXPECT_EQ(password.children_nodes.size(), 0u);

  // Now reveal the password (simulating clicking the eye icon)
  // This mimics JavaScript: passwordInput.type = 'text';
  Document* document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
  auto* input_element =
      To<HTMLInputElement>(document->getElementById(AtomicString("pwd")));
  ASSERT_TRUE(input_element);
  input_element->setType(AtomicString("text"));

  // Ensure the DOM is updated
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Get AI page content again - password should still be hidden
  GetAIPageContent();

  const auto& root2 = ContentRootNode();
  EXPECT_EQ(root2.children_nodes.size(), 1u);

  const auto& form2 = *root2.children_nodes[0];
  EXPECT_EQ(form2.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kForm);
  EXPECT_EQ(form2.children_nodes.size(), 1u);

  const auto& revealed_password = *form2.children_nodes[0];
  CheckFormControlNode(revealed_password,
                       mojom::blink::FormControlType::kInputText);
  EXPECT_EQ(revealed_password.content_attributes->form_control_data->field_name,
            "Enter password");
  // Even though the password is revealed, the value should still be hidden
  // because HasBeenBeenPasswordField() is true.
  EXPECT_EQ(
      revealed_password.content_attributes->form_control_data->field_value,
      nullptr);
  EXPECT_EQ(revealed_password.content_attributes->form_control_data
                ->redaction_decision,
            mojom::AIPageContentRedactionDecision::kRedacted_HasBeenPassword);
  EXPECT_EQ(revealed_password.children_nodes.size(), 0u);

  input_element->SetValue("");

  // Ensure the DOM is updated
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // Get AI page content again - empty passwords are unredacted.
  GetAIPageContent();

  const auto& root3 = ContentRootNode();
  EXPECT_EQ(root3.children_nodes.size(), 1u);

  const auto& form3 = *root3.children_nodes[0];
  EXPECT_EQ(form3.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kForm);
  EXPECT_EQ(form3.children_nodes.size(), 1u);

  const auto& empty_password = *form3.children_nodes[0];
  CheckFormControlNode(empty_password,
                       mojom::blink::FormControlType::kInputText);
  EXPECT_EQ(empty_password.content_attributes->form_control_data->field_name,
            "Enter password");
  EXPECT_EQ(empty_password.content_attributes->form_control_data->field_value,
            "");
  EXPECT_EQ(
      empty_password.content_attributes->form_control_data->redaction_decision,
      mojom::AIPageContentRedactionDecision::kUnredacted_EmptyPassword);
  EXPECT_EQ(empty_password.children_nodes.size(), 1u);
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
  EXPECT_EQ(text_area.content_attributes->form_control_data->redaction_decision,
            mojom::AIPageContentRedactionDecision::kNoRedactionNecessary);
  CheckHitTestableAndInteractive(text_area,
                                 {ClickabilityReason::kClickableControl});

  // Text area uses a UA shadow DOM internally to create an editable box.
  const auto& shadow_div = *text_area.children_nodes[0];
  EXPECT_EQ(shadow_div.children_nodes.size(), 1u);
  CheckContainerNode(shadow_div);
  CheckHitTestableAndInteractive(shadow_div, {ClickabilityReason::kEditable});

  EXPECT_EQ(shadow_div.children_nodes.size(), 1u);
  const auto& text_area_text = *shadow_div.children_nodes[0];
  CheckTextNode(text_area_text, "text");
  CheckHitTestableButNotInteractive(text_area_text);
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
  EXPECT_EQ(button.content_attributes->form_control_data->redaction_decision,
            mojom::AIPageContentRedactionDecision::kNoRedactionNecessary);
  CheckHitTestableAndInteractive(button,
                                 {ClickabilityReason::kClickableControl});
  EXPECT_TRUE(button.content_attributes->node_interaction_info->is_focusable);

  ASSERT_EQ(button.children_nodes.size(), 1u);
  const auto& button_text = *button.children_nodes[0];
  CheckTextNode(button_text, "button");
  EXPECT_TRUE(button_text.content_attributes->node_interaction_info);
  CheckHitTestableButNotInteractive(button_text);
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
  CheckHitTestableButNotInteractive(resize);

  ASSERT_EQ(resize.children_nodes.size(), 1u);
  const auto& resize_text = *resize.children_nodes[0];
  CheckTextNode(resize_text, "resize");
  EXPECT_TRUE(resize_text.content_attributes->node_interaction_info);
  CheckHitTestableButNotInteractive(resize_text);
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

  EXPECT_TRUE(iframe.content_attributes->iframe_data->content);
  const auto& iframe_interaction_info =
      iframe.content_attributes->iframe_data->content->get_local_frame_data()
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

TEST_F(AIPageContentAgentTest, AccessibilityFocus) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    #button1 {"
      "      position: absolute;"
      "      top: -10px;"
      "      left: -20px;"
      "      width: 30px;"
      "      height: 40px;"
      "    }"
      "  </style>"
      "  <button id='button1'>button1</button>"
      "  <div id='div2'>div2</div>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  // Enable accessibility.
  ui::AXMode ax_mode = ui::kAXModeComplete;
  Document* document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
  auto context = std::make_unique<AXContext>(*document, ax_mode);
  EXPECT_TRUE(document->ExistingAXObjectCache());
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document->ExistingAXObjectCache());
  EXPECT_EQ(ax_mode, ax_object_cache->GetAXMode());
  ax_object_cache->UpdateAXForAllDocuments();

  // Set accessibility focus to the button.
  auto* button_element = document->getElementById(AtomicString("button1"));
  auto* button_ax_object = ax_object_cache->Get(button_element);
  ui::AXActionData action_data;
  action_data.action = ax::mojom::blink::Action::kSetAccessibilityFocus;
  button_ax_object->PerformAction(action_data);

  GetAIPageContent();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 2u);

  const auto& button = *root.children_nodes[0];
  const auto& div2 = *root.children_nodes[1];
  const auto& page_interaction_info = Content()->page_interaction_info;
  EXPECT_EQ(page_interaction_info->accessibility_focused_dom_node_id,
            button.content_attributes->dom_node_id);
  CheckGeometry(button, gfx::Rect(-20, -10, 30, 40), gfx::Rect(0, 0, 10, 30));
  EXPECT_FALSE(div2.content_attributes->geometry);
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
      ->setAttribute(html_names::kContentAttr, g_null_atom);

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

  EXPECT_TRUE(iframe.content_attributes->iframe_data->content);
  EXPECT_EQ(iframe_data.content->get_local_frame_data()->meta_data.size(), 2u);
  EXPECT_EQ(iframe_data.content->get_local_frame_data()->meta_data[0]->name,
            "author");
  EXPECT_EQ(iframe_data.content->get_local_frame_data()->meta_data[0]->content,
            "Gary");

  EXPECT_EQ(iframe_data.content->get_local_frame_data()->meta_data[1]->name,
            "keywords");
  EXPECT_EQ(iframe_data.content->get_local_frame_data()->meta_data[1]->content,
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
  EXPECT_TRUE(iframe_data.content);
  EXPECT_EQ(iframe_data.content->get_local_frame_data()->meta_data.size(), 1u);

  EXPECT_EQ(iframe_data.content->get_local_frame_data()->meta_data[0]->name,
            "author");
  EXPECT_EQ(iframe_data.content->get_local_frame_data()->meta_data[0]->content,
            "Gary");

  EXPECT_EQ(iframe.children_nodes.size(), 1u);

  // In the iframe children_nodes there is a root node that has two children.
  // The first child is a text node and the second is the subiframe.  The key
  // thing we want to check here is that the subiframe has the correct meta
  // data.
  const auto& subiframe = *iframe.children_nodes[0]->children_nodes[1];
  EXPECT_EQ(subiframe.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kIframe);

  const auto& subiframe_data = *subiframe.content_attributes->iframe_data;
  EXPECT_TRUE(subiframe_data.content);
  EXPECT_EQ(subiframe_data.content->get_local_frame_data()->meta_data.size(),
            1u);

  EXPECT_EQ(subiframe_data.content->get_local_frame_data()->meta_data[0]->name,
            "author");
  EXPECT_EQ(
      subiframe_data.content->get_local_frame_data()->meta_data[0]->content,
      "Jordan");
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
  EXPECT_TRUE(iframe->content_attributes->iframe_data->content);
  EXPECT_TRUE(
      iframe->content_attributes->iframe_data->content->get_local_frame_data()
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
  EXPECT_TRUE(iframe1->content_attributes->iframe_data->content);
  EXPECT_TRUE(
      iframe1->content_attributes->iframe_data->content->get_local_frame_data()
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
  EXPECT_TRUE(iframe2->content_attributes->iframe_data->content);
  EXPECT_FALSE(
      iframe2->content_attributes->iframe_data->content->get_local_frame_data()
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
  EXPECT_TRUE(iframe3->content_attributes->iframe_data->content);
  EXPECT_TRUE(
      iframe3->content_attributes->iframe_data->content->get_local_frame_data()
          ->contains_paid_content);

  const auto& children3 = iframe3->children_nodes[0]->children_nodes;
  EXPECT_FALSE(
      ContainsRole(children3[0]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
  EXPECT_TRUE(
      ContainsRole(children3[1]->content_attributes->annotated_roles,
                   mojom::blink::AIPageContentAnnotatedRole::kPaidContent));
}

TEST_F(AIPageContentAgentTest, AnchorInInlineWithFloatingSibling) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<!DOCTYPE html>"
      "<body style='margin:0; font: 1px/1px Ahem'>"
      "  <span>"
      "  <a href='https://www.google.com'>"
      "    <div style='position: relative; float: left;'>text in div</div>"
      "    <span>text</span>"
      "  </a>"
      "  </span>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  LoadAhem();
  GetAIPageContentWithActionableElements();

  const mojom::blink::AIPageContentNode* anchor = FindNodeBySelector("a");
  ASSERT_TRUE(anchor->content_attributes->geometry);
  CheckAnchorNode(*anchor, blink::KURL("https://www.google.com/"), {});
  ASSERT_TRUE(anchor->content_attributes->node_interaction_info);
  EXPECT_TRUE(anchor->content_attributes->node_interaction_info
                  ->document_scoped_z_order);
  CheckGeometry(*anchor, gfx::Rect(11, 0, 4, 1), gfx::Rect(11, 0, 4, 1));
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

  GetAIPageContentWithActionableElements();

  const auto& section = ContentRootNode().children_nodes[0];
  CheckAnnotatedRole(*section,
                     mojom::blink::AIPageContentAnnotatedRole::kSection);

  // Although blurring causes the rectangle to show outside of the 100x100 rect,
  // the extra area is not hit testable, and is therefore not included in the
  // visible bounding box.
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

  GetAIPageContentWithActionableElements();

  const auto& section = ContentRootNode().children_nodes[0];
  CheckAnnotatedRole(*section,
                     mojom::blink::AIPageContentAnnotatedRole::kSection);

  CheckGeometry(*section, gfx::Rect(200, 200, 100, 100),
                gfx::Rect(200, 200, 100, 100));
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

  GetAIPageContentWithActionableElements();

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
  CheckHitTestableAndInteractive(cursor, {ClickabilityReason::kCursorPointer});

  const auto& no_click = *cursor.children_nodes[0];
  EXPECT_TRUE(no_click.content_attributes->node_interaction_info);
  CheckHitTestableButNotInteractive(no_click);

  const auto& click = *cursor.children_nodes[1];
  EXPECT_TRUE(click.content_attributes->node_interaction_info);
  CheckHitTestableAndInteractive(click, {ClickabilityReason::kCursorPointer});

  const auto& article = *ContentRootNode().children_nodes[1];
  EXPECT_TRUE(article.content_attributes->node_interaction_info);
  CheckHitTestableButNotInteractive(article);
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
  CheckHitTestableAndInteractive(valid, {ClickabilityReason::kCursorPointer});

  const auto& invalid = *ContentRootNode().children_nodes[1];
  EXPECT_TRUE(invalid.content_attributes->node_interaction_info);
  CheckHitTestableButNotInteractive(invalid);
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
  CheckHitTestableAndInteractive(input,
                                 {ClickabilityReason::kClickableControl});
  EXPECT_EQ(input.content_attributes->form_control_data->redaction_decision,
            mojom::AIPageContentRedactionDecision::kNoRedactionNecessary);

  const auto& label = *root.children_nodes[1];
  CheckContainerNode(label);
  ASSERT_TRUE(label.content_attributes->node_interaction_info);
  EXPECT_TRUE(label.content_attributes->node_interaction_info
                  ->clickability_reasons.empty());
  EXPECT_EQ(label.content_attributes->label_for_dom_node_id,
            input.content_attributes->dom_node_id);
}

TEST_F(AIPageContentAgentTest, LabelGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<style>body * { margin:0; padding:0; font:1px/1px 'Ahem'; }</style>"
      "<body style='margin: 3px;'>"
      " <label for='myCheckbox'>1234567890</label>"
      " <input type='checkbox' id='myCheckbox' "
      "        style='width:1px;height:1px;appearance:none;'/>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  LoadAhem();

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 2u);

  const auto& label = *root.children_nodes[0];
  CheckContainerNode(label);
  ASSERT_TRUE(label.content_attributes->node_interaction_info);
  EXPECT_TRUE(label.content_attributes->node_interaction_info
                  ->clickability_reasons.empty());

  const auto& input = *root.children_nodes[1];
  CheckFormControlNode(input, mojom::blink::FormControlType::kInputCheckbox);
  ASSERT_TRUE(input.content_attributes->node_interaction_info);
  CheckHitTestableAndInteractive(input,
                                 {ClickabilityReason::kClickableControl});
  EXPECT_EQ(input.content_attributes->form_control_data->redaction_decision,
            mojom::AIPageContentRedactionDecision::kNoRedactionNecessary);

  EXPECT_EQ(label.content_attributes->label_for_dom_node_id,
            input.content_attributes->dom_node_id);

  ASSERT_TRUE(label.content_attributes->geometry);
  const auto& label_geometry = *label.content_attributes->geometry;
  // With the 1px Ahem font loaded above, each glyph occupies a 1x1 cell so the
  // 10-character label spans exactly 10x1 CSS pixels.
  EXPECT_EQ(label_geometry.outer_bounding_box, gfx::Rect(3, 3, 10, 1));
  EXPECT_EQ(label_geometry.visible_bounding_box,
            label_geometry.outer_bounding_box);
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
  CheckHitTestableButNotInteractive(label);

  const auto& input = *label.children_nodes[0];
  CheckFormControlNode(input, mojom::blink::FormControlType::kInputCheckbox);
  ASSERT_TRUE(input.content_attributes->node_interaction_info);
  CheckHitTestableAndInteractive(input,
                                 {ClickabilityReason::kClickableControl});
  EXPECT_EQ(label.content_attributes->label_for_dom_node_id,
            input.content_attributes->dom_node_id);

  CheckTextNode(*label.children_nodes[1], "Check me!");
}

TEST_F(AIPageContentAgentTest, SVGWithText) {
  ScopedAIPageContentIncludeSVGSubtreeForTest scoped_feature(
      /*enabled=*/true);

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
            mojom::blink::AIPageContentAttributeType::kSvgRoot);
  ASSERT_TRUE(svg.content_attributes->svg_root_data);
  EXPECT_EQ(svg.content_attributes->svg_root_data->inner_text,
            "Hello SVG Text!");

  const auto& text_child = *svg.children_nodes[0];
  EXPECT_EQ(text_child.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kText);
  // Note that whitespace is kept.
  CheckTextNode(text_child, "      Hello SVG Text!    ");
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
            mojom::blink::AIPageContentAttributeType::kSvgRoot);
  ASSERT_TRUE(svg.content_attributes->svg_root_data);
  EXPECT_FALSE(svg.content_attributes->svg_root_data->inner_text);

  // Only visible text nodes are extracted.
  EXPECT_EQ(svg.children_nodes.size(), 0u);
}

TEST_F(AIPageContentAgentTest, SVGSubtreeContainersAreKept) {
  ScopedAIPageContentIncludeSVGSubtreeForTest scoped_feature(
      /*enabled=*/true);

  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <svg width='400' height='200'>"
      "    <a href='example.html'>"
      "      <image/>"
      "      <rect width='10%' height='10%' fill='red'/>"
      "      <text x='50%' y='50/%' font-size='24'>"
      "        Hello SVG Text!"
      "      </text>"
      "    </a>"
      "  </svg>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));
  auto& document = *helper_.LocalMainFrame()->GetFrame()->GetDocument();
  document.getElementsByTagName(AtomicString("image"))
      ->item(0)
      ->setAttribute(html_names::kSrcAttr, AtomicString(kSmallImage));

  GetAIPageContentWithActionableElements();

  const auto& svg = *ContentRootNode().children_nodes[0];
  EXPECT_EQ(svg.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kSvgRoot);
  ASSERT_TRUE(svg.content_attributes->svg_root_data);
  EXPECT_EQ(svg.content_attributes->svg_root_data->inner_text,
            "Hello SVG Text!");

  ASSERT_EQ(svg.children_nodes.size(), 1u);
  const auto& a_child = *svg.children_nodes[0];
  EXPECT_EQ(a_child.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kContainer);
  ASSERT_TRUE(a_child.content_attributes->node_interaction_info);
  EXPECT_FALSE(a_child.content_attributes->node_interaction_info
                   ->clickability_reasons.empty());

  ASSERT_EQ(a_child.children_nodes.size(), 3u);

  const auto& image_child = *a_child.children_nodes[0];
  EXPECT_EQ(image_child.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kImage);

  // Non-text, non-image elements are processed as generic containers.
  const auto& rect_child = *a_child.children_nodes[1];
  EXPECT_EQ(rect_child.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kContainer);

  const auto& text_child = *a_child.children_nodes[2];
  EXPECT_EQ(text_child.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kText);
  // Note that whitespace is kept.
  CheckTextNode(text_child, "        Hello SVG Text!      ");
  ASSERT_TRUE(text_child.content_attributes->node_interaction_info);
  EXPECT_TRUE(text_child.content_attributes->node_interaction_info
                  ->clickability_reasons.empty());
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
  EXPECT_TRUE(button.content_attributes->node_interaction_info->is_disabled);
  EXPECT_THAT(
      button.content_attributes->node_interaction_info
          ->interaction_disabled_reasons,
      testing::UnorderedElementsAre(InteractionDisabledReason::kDisabled));
}

TEST_F(AIPageContentAgentTest, InertButton) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
        <body>
         <button inert>Text</button>
        </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);
  const auto& button = *root.children_nodes.at(0);
  EXPECT_FALSE(button.content_attributes->node_interaction_info);
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
  CheckHitTestableAndInteractive(a, {ClickabilityReason::kCursorPointer});

  EXPECT_EQ(a.children_nodes.size(), 1u);
  const auto& before = *a.children_nodes[0];
  ASSERT_TRUE(before.content_attributes->node_interaction_info);
  CheckHitTestableAndInteractive(before, {ClickabilityReason::kCursorPointer});
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
  CheckHitTestableAndInteractive(a, {ClickabilityReason::kCursorPointer});

  EXPECT_EQ(a.children_nodes.size(), 1u);
  const auto& before = *a.children_nodes[0];
  ASSERT_TRUE(before.content_attributes->node_interaction_info);
  CheckHitTestableButNotInteractive(before);
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
  CheckHitTestableAndInteractive(a, {ClickabilityReason::kCursorPointer});

  EXPECT_EQ(a.children_nodes.size(), 1u);
  const auto& text = *a.children_nodes[0];
  CheckTextNode(text, "hello");
  EXPECT_FALSE(text.content_attributes->node_interaction_info);
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
  EXPECT_TRUE(section.content_attributes->node_interaction_info->is_disabled);
  EXPECT_THAT(
      section.content_attributes->node_interaction_info
          ->interaction_disabled_reasons,
      testing::UnorderedElementsAre(InteractionDisabledReason::kAriaDisabled));

  // The child is also not actionable.
  ASSERT_EQ(section.children_nodes.size(), 1u);
  const auto& input = *section.children_nodes.at(0);
  CheckHitTestableButNotInteractive(input);
  // Parent element `aria-disable` value overrides child element's.
  EXPECT_TRUE(input.content_attributes->node_interaction_info->is_disabled);
  EXPECT_THAT(
      input.content_attributes->node_interaction_info
          ->interaction_disabled_reasons,
      testing::UnorderedElementsAre(InteractionDisabledReason::kAriaDisabled));
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
  EXPECT_TRUE(fieldset.content_attributes->node_interaction_info->is_disabled);
  EXPECT_THAT(
      fieldset.content_attributes->node_interaction_info
          ->interaction_disabled_reasons,
      testing::UnorderedElementsAre(InteractionDisabledReason::kDisabled));

  const auto& button = *fieldset.children_nodes.at(0);
  CheckHitTestableButNotInteractive(button);
  EXPECT_TRUE(button.content_attributes->node_interaction_info->is_disabled);
  EXPECT_THAT(
      button.content_attributes->node_interaction_info
          ->interaction_disabled_reasons,
      testing::UnorderedElementsAre(InteractionDisabledReason::kDisabled));
}

TEST_F(AIPageContentAgentTest, Fieldset) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body>
        <form>
          <fieldset>
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
}

TEST_F(AIPageContentAgentTest, ShadowDOMInInput) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body>
        <input type=range></input>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& input = *root.children_nodes.at(0);
  ASSERT_TRUE(input.content_attributes->node_interaction_info);
  CheckHitTestableAndInteractive(input,
                                 {ClickabilityReason::kClickableControl});

  EXPECT_NE(input.children_nodes.size(), 0u);
  const auto& shadow_div = *input.children_nodes.at(0);
  CheckHitTestableButNotInteractive(shadow_div);
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
        <div role="button">hello</div>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);

  const auto& button = *root.children_nodes.at(0);
  ASSERT_TRUE(button.content_attributes->node_interaction_info);
  CheckHitTestableAndInteractive(button, {ClickabilityReason::kAriaRole});
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
  CheckHitTestableAndInteractive(button,
                                 {ClickabilityReason::kClickableControl});

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
  CheckHitTestableButNotInteractive(label);

  const auto& select = *root.children_nodes.at(1);
  ASSERT_TRUE(select.content_attributes->node_interaction_info);
  CheckHitTestableAndInteractive(select,
                                 {ClickabilityReason::kClickableControl,
                                  ClickabilityReason::kHoverPseudoClass});
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonClickableControl) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><button id='testButton'>Click Me</button></body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& button_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(button_node.content_attributes->node_interaction_info);
  EXPECT_THAT(
      button_node.content_attributes->node_interaction_info
          ->clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kClickableControl));
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonClickEvents) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><div id='testDiv'>Clickable</div>"
      "<script>document.getElementById('testDiv').onclick = "
      "function(){};</script>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);
  EXPECT_THAT(
      div_node.content_attributes->node_interaction_info->clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kClickEvents));
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonMouseHover) {
  // An element with various mouse event listeners.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><div id='testDiv'>Mouse Events</div>"
      "<script>"
      "  const div = document.getElementById('testDiv');"
      "  div.onmouseover = function(){};"
      "</script>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);
  EXPECT_THAT(
      div_node.content_attributes->node_interaction_info->clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kMouseHover));
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonMouseClick) {
  // An element with various mouse event listeners.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><div id='testDiv'>Mouse Events</div>"
      "<script>"
      "  const div = document.getElementById('testDiv');"
      "  div.onmousedown = function(){};"
      "</script>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);
  EXPECT_THAT(
      div_node.content_attributes->node_interaction_info->clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kMouseClick));
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonKeyEvents) {
  // An element with keyboard event listeners.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><input type='text' id='testInput'>"
      "<script>"
      "  const input = document.getElementById('testInput');"
      "  input.onkeydown = function(){};"
      "</script>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& input_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(input_node.content_attributes->node_interaction_info);
  EXPECT_THAT(input_node.content_attributes->node_interaction_info
                  ->clickability_reasons,
              testing::Contains(
                  mojom::blink::AIPageContentClickabilityReason::kKeyEvents));
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonEditable) {
  // A div with contenteditable attribute.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><div contenteditable='true'>Editable Content</div></body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);
  EXPECT_THAT(
      div_node.content_attributes->node_interaction_info->clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kEditable));
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonCursorPointer) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><div style='cursor: pointer;'>Pointer Cursor</div></body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);
  EXPECT_THAT(
      div_node.content_attributes->node_interaction_info->clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kCursorPointer));
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonAriaRole) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), "<body><div role='link'>ARIA Link</div></body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);
  EXPECT_EQ(div_node.content_attributes->aria_role,
            ax::mojom::blink::Role::kLink);
  EXPECT_THAT(
      div_node.content_attributes->node_interaction_info->clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kAriaRole));
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonMultipleReasons) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "<button id='multiReasonBtn' contenteditable='true' style='cursor: "
      "pointer;' role='menuitem'>"
      "Multi-Reason Button"
      "</button>"
      "<script>"
      "  const btn = document.getElementById('multiReasonBtn');"
      "  btn.onclick = function(){};"
      "  btn.onmouseover = function(){};"
      "  btn.onmouseup = function(){};"
      "  btn.onkeydown = function(){};"
      "</script>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& button_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(button_node.content_attributes->node_interaction_info);
  EXPECT_THAT(
      button_node.content_attributes->node_interaction_info
          ->clickability_reasons,
      testing::UnorderedElementsAre(
          mojom::blink::AIPageContentClickabilityReason::kClickableControl,
          mojom::blink::AIPageContentClickabilityReason::kClickEvents,
          mojom::blink::AIPageContentClickabilityReason::kMouseHover,
          mojom::blink::AIPageContentClickabilityReason::kMouseClick,
          mojom::blink::AIPageContentClickabilityReason::kKeyEvents,
          mojom::blink::AIPageContentClickabilityReason::kEditable,
          mojom::blink::AIPageContentClickabilityReason::kCursorPointer,
          mojom::blink::AIPageContentClickabilityReason::kAriaRole));
}

TEST_F(AIPageContentAgentTest, ClickabilityReasonNoReasons) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), "<body><div>Plain Div</div></body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);
  EXPECT_TRUE(div_node.content_attributes->node_interaction_info
                  ->clickability_reasons.empty());
}

TEST_F(AIPageContentAgentTest, AriaHasPopup) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><div aria-haspopup=true>Plain Div</div></body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);

  const auto& interaction_info =
      *div_node.content_attributes->node_interaction_info;
  EXPECT_THAT(
      interaction_info.clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kAriaHasPopup));
}

TEST_F(AIPageContentAgentTest, AriaExpandedTrue) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><div aria-expanded=true>Plain Div</div></body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);

  const auto& interaction_info =
      *div_node.content_attributes->node_interaction_info;
  EXPECT_THAT(
      interaction_info.clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kAriaExpandedTrue));
}

TEST_F(AIPageContentAgentTest, AriaExpandedFalse) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><div aria-expanded=false>Plain Div</div></body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);

  const auto& interaction_info =
      *div_node.content_attributes->node_interaction_info;
  EXPECT_THAT(
      interaction_info.clickability_reasons,
      testing::Contains(
          mojom::blink::AIPageContentClickabilityReason::kAriaExpandedFalse));
}

TEST_F(AIPageContentAgentTest, Autocomplete) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"(<body>
      <input>
      <input autocomplete=off>
      <input autocomplete=on>
      <input aria-autocomplete>
      <input aria-autocomplete=none>
      <input aria-autocomplete=list>
      </body>)",
      url_test_helpers::ToKURL("http://foobar.com"));

  const bool kExpected[] = {
      false,  // no attribute
      false,  // disabled
      true,
      false,  // empty
      false,  // disabled
      true,
  };

  GetAIPageContentWithActionableElements();

  for (int i = 0; bool expected : kExpected) {
    SCOPED_TRACE(i);

    const auto& input_node = *ContentRootNode().children_nodes[i];
    ASSERT_TRUE(input_node.content_attributes->node_interaction_info);
    const auto& interaction_info =
        *input_node.content_attributes->node_interaction_info;

    EXPECT_THAT(
        interaction_info.clickability_reasons,
        testing::Contains(
            mojom::blink::AIPageContentClickabilityReason::kAutocomplete)
            .Times(expected));

    ++i;
  }
}

TEST_F(AIPageContentAgentTest, TabIndex) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), "<body><div tabindex=0>Plain Div</div></body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();
  const auto& div_node = *ContentRootNode().children_nodes[0];
  ASSERT_TRUE(div_node.content_attributes->node_interaction_info);

  const auto& interaction_info =
      *div_node.content_attributes->node_interaction_info;
  EXPECT_THAT(interaction_info.clickability_reasons,
              testing::Contains(
                  mojom::blink::AIPageContentClickabilityReason::kTabIndex));
}

TEST_F(AIPageContentAgentTest, ClipPathCircle) {
  // The <div> element is clipped to a small circle.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"(
      <body>
        <style>
          div {
            position: absolute;
            top: 0;
            left: 0;
            width: 100px;
            height: 100px;
            background: red;
            clip-path: circle(20%);
          }
        </style>
        <div onclick=console.log(1)></div>
      </body>)",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];

  CheckGeometry(div_node, gfx::Rect(30, 30, 40, 40), gfx::Rect(30, 30, 40, 40));
}

TEST_F(AIPageContentAgentTest, ClipPathEllipse) {
  // The <div> element is clipped to an ellipse.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"(
      <body>
        <style>
          div {
            position: absolute;
            top: 0;
            left: 0;
            width: 100px;
            height: 100px;
            background: red;
            clip-path: ellipse(20px 50px);
          }
        </style>
        <div onclick=console.log(1)></div>
      </body>)",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];

  CheckGeometry(div_node, gfx::Rect(30, 0, 40, 100), gfx::Rect(30, 0, 40, 100));
}

TEST_F(AIPageContentAgentTest, ClipPathCircleHalf) {
  // Only the bottom half of the circle is within the viewport.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"(
      <body>
        <style>
          div {
            position: absolute;
            top: -50px;
            left: 0;
            width: 100px;
            height: 100px;
            background: red;
            clip-path: circle(20%);
          }
        </style>
        <div onclick=console.log(1)></div>
      </body>)",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];

  CheckGeometry(div_node, gfx::Rect(30, -20, 40, 40), gfx::Rect(30, 0, 40, 20));
}

TEST_F(AIPageContentAgentTest, ClipPathEmpty) {
  // The entire <div> element is clipped.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"(
      <body>
        <style>
          body { margin: 0; }
          div {
            position: absolute;
            top: 200px;
            left: 300px;
            width: 100px;
            height: 100px;
            background: red;
            clip-path: circle(0 at left top);
          }
        </style>
        <div onclick=console.log(1)></div>
      </body>)",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];

  const auto& geometry = *div_node.content_attributes->geometry;
  EXPECT_TRUE(geometry.outer_bounding_box.IsEmpty());
  EXPECT_TRUE(geometry.visible_bounding_box.IsEmpty());
}

TEST_F(AIPageContentAgentTest, InlineWithFloatAndInlineContentGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <body>
        <style>
          body { margin: 0; font: 10px/10px Ahem; }
          a { position: absolute; top: 0; left: 0; color: black; }
          #floater { float: left; width: 30px; height: 10px; background: #ccc; }
        </style>
        <a id="target" href="#"><span id="floater"></span>XX</a>
      </body>)HTML",
      url_test_helpers::ToKURL("http://example.com"));

  LoadAhem();

  GetAIPageContentWithActionableElements();

  Document* document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
  ASSERT_TRUE(document);
  Element* anchor_element = document->getElementById(AtomicString("target"));
  ASSERT_TRUE(anchor_element);
  LayoutObject* anchor_layout_object = anchor_element->GetLayoutObject();
  ASSERT_TRUE(anchor_layout_object);
  const auto* anchor_box =
      DynamicTo<LayoutBoxModelObject>(anchor_layout_object);
  ASSERT_TRUE(anchor_box);

  const mojom::blink::AIPageContentNode* anchor_node =
      FindNodeBySelector("#target");

  ASSERT_TRUE(anchor_node);
  ASSERT_TRUE(anchor_node->content_attributes->geometry);
  const auto& anchor_geometry = *anchor_node->content_attributes->geometry;

  EXPECT_EQ(gfx::Rect(0, 0, 50, 10), anchor_geometry.outer_bounding_box);
  EXPECT_EQ(gfx::Rect(0, 0, 50, 10), anchor_geometry.visible_bounding_box);

  const mojom::blink::AIPageContentNode* floater_node =
      FindNodeBySelector("#floater");
  ASSERT_TRUE(floater_node);
  ASSERT_TRUE(floater_node->content_attributes->geometry);
  const auto& floater_geometry = *floater_node->content_attributes->geometry;

  EXPECT_EQ(gfx::Rect(0, 0, 30, 10), floater_geometry.outer_bounding_box);
  EXPECT_EQ(gfx::Rect(0, 0, 30, 10), floater_geometry.visible_bounding_box);
}

TEST_F(AIPageContentAgentTest, ActionableModeKeepsContainerQuadGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <body style="margin: 0; font: 10px/10px Ahem;">
        <details id="wrapper">
          <summary>Expandable</summary>
          Contents.
        </details>
      </body>)HTML",
      url_test_helpers::ToKURL("http://example.com"));

  LoadAhem();

  GetAIPageContentWithActionableElements();

  Document* document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
  ASSERT_TRUE(document);
  Element* wrapper_element = document->getElementById(AtomicString("wrapper"));
  ASSERT_TRUE(wrapper_element);
  DOMNodeId wrapper_dom_node_id = DOMNodeIds::IdForNode(wrapper_element);

  const auto* wrapper_node = FindNodeByDomNodeId(wrapper_dom_node_id);
  ASSERT_TRUE(wrapper_node);
  CheckContainerNode(*wrapper_node);
  ASSERT_TRUE(wrapper_node->content_attributes->geometry);
  const auto& geometry = *wrapper_node->content_attributes->geometry;

  LayoutObject* wrapper_layout_object = wrapper_element->GetLayoutObject();
  ASSERT_TRUE(wrapper_layout_object);
  const auto* wrapper_box =
      DynamicTo<LayoutBoxModelObject>(wrapper_layout_object);
  ASSERT_TRUE(wrapper_box);

  EXPECT_EQ(gfx::Rect(0, 0, 1000, 10), geometry.outer_bounding_box);
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 10), geometry.visible_bounding_box);
}

TEST_F(AIPageContentAgentTest, InlineWithFloatInlineBoxUnionGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"HTML(
      <body style="margin: 0; font: 10px/10px Ahem;">
        <a id="inline-float-union">
          <span id="inline-wrapper">
            <span id="floated" class="inner-float">1</span>
            <span id="inline-box" style="display:inline-block; width: 4px; height: 20px;">2</span>
          </span>
        </a>
        <style>
          .inner-float {
            width: 20px;
            height: 20px;
            display: block;
            float: left;
          }
        </style>
      </body>)HTML",
      url_test_helpers::ToKURL("http://example.com"));

  LoadAhem();

  GetAIPageContentWithActionableElements();

  Document* document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
  ASSERT_TRUE(document);

  // As there is no clipping for either node, the outer box == visible box.

  {
    const auto* anchor_node = FindNodeBySelector("#inline-float-union");
    ASSERT_TRUE(anchor_node);
    ASSERT_TRUE(anchor_node->content_attributes);
    ASSERT_TRUE(anchor_node->content_attributes->geometry);

    const auto& anchor_geometry = *anchor_node->content_attributes->geometry;
    EXPECT_EQ(anchor_geometry.outer_bounding_box,
              anchor_geometry.visible_bounding_box);
  }

  {
    const auto* container_node = FindNodeBySelector("#inline-wrapper");
    ASSERT_TRUE(container_node);
    ASSERT_TRUE(container_node->content_attributes);
    ASSERT_TRUE(container_node->content_attributes->geometry);
    const auto& container_geometry =
        *container_node->content_attributes->geometry;
    EXPECT_EQ(container_geometry.outer_bounding_box,
              container_geometry.visible_bounding_box);
  }
}

TEST_F(AIPageContentAgentTest, InlineWithOnlyFloatGeometry) {
  // An inline anchor with no inline text content but containing a floated
  // descendant should not inherit geometry from the float.
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"(
      <body>
        <style>
          body { margin: 0; }
          a { position: absolute; left: 0; top: 0; }
          #floater { float: left; width: 20px; height: 10px; background: #000; }
        </style>
        <a href="#"><span><span id="floater"></span></span></a>
      </body>)",
      url_test_helpers::ToKURL("http://example.com"));

  // Use actionable mode so the anchor is included in the APC tree.
  GetAIPageContentWithActionableElements();

  // Find the first node that has anchor data (iterative DFS, no std::function).
  const auto& root = ContentRootNode();
  const mojom::blink::AIPageContentNode* anchor_node = nullptr;
  Vector<const mojom::blink::AIPageContentNode*> stack;
  stack.push_back(&root);
  while (!stack.empty()) {
    const auto* node = stack.back();
    stack.pop_back();
    if (node->content_attributes && node->content_attributes->anchor_data) {
      anchor_node = node;
      break;
    }
    // Push children in reverse so traversal order matches natural order.
    for (size_t i = node->children_nodes.size(); i > 0; --i) {
      stack.push_back(node->children_nodes[i - 1].get());
    }
  }
  ASSERT_TRUE(anchor_node);

  ASSERT_TRUE(anchor_node->content_attributes->geometry);
  const auto& anchor_geom = *anchor_node->content_attributes->geometry;

  // Inline elements that keep their own fragment should retain geometry even
  // when their only ink comes from a floating descendant.
  EXPECT_EQ(anchor_geom.outer_bounding_box, gfx::Rect(0, 0, 20, 10));
  EXPECT_EQ(anchor_geom.visible_bounding_box, gfx::Rect(0, 0, 20, 10));
}

TEST_F(AIPageContentAgentTest, StructuralWrapperWithoutPaintGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body style="margin: 0; font: 10px/10px Ahem;">
        <a id="wrapper" href="#" style="position: relative; display: inline-block;">
          <span id="child" style="position: absolute; left: 0; top: 0;
                                   width: 10px; height: 10px;
                                   background: black; color: white;">X</span>
        </a>
      </body>)HTML",
      url_test_helpers::ToKURL("http://example.com"));

  LoadAhem();

  GetAIPageContentWithActionableElements();

  auto* wrapper_node = FindNodeBySelector("#wrapper");
  ASSERT_TRUE(wrapper_node);
  EXPECT_TRUE(wrapper_node->content_attributes->anchor_data);
  ASSERT_TRUE(wrapper_node->content_attributes->geometry);
  const auto& wrapper_geometry = *wrapper_node->content_attributes->geometry;
  EXPECT_TRUE(wrapper_geometry.visible_bounding_box.IsEmpty());
  EXPECT_TRUE(wrapper_geometry.outer_bounding_box.IsEmpty());

  auto* child_node = FindNodeBySelector("#child");
  ASSERT_TRUE(child_node);
  ASSERT_TRUE(child_node->content_attributes->geometry);
  const auto& child_geometry = *child_node->content_attributes->geometry;
  EXPECT_FALSE(child_geometry.visible_bounding_box.IsEmpty());
  EXPECT_FALSE(child_geometry.outer_bounding_box.IsEmpty());
}

TEST_F(AIPageContentAgentTest, InlineBlockFixedDescendantKeepsGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <style>
        body { margin: 0; font: 10px/10px Ahem; }
        .content { padding: 60px 0 0 40px; }
        a.inline-block-anchor {
          display: inline-block;
          width: 100px;
          height: 100px;
          border: 1px solid black;
        }
        .fixed-box {
          position: fixed;
          top: 0;
          left: 0;
          width: 200px;
          height: 25px;
        }
      </style>
      <body>
        <div class="content">
          <a id="anchor" class="inline-block-anchor" href="#test">
            <div class="fixed-box">a</div>
          </a>
        </div>
      </body>)HTML",
      url_test_helpers::ToKURL("http://example.com"));

  GetAIPageContentWithActionableElements();

  const auto* anchor_node = FindNodeBySelector("#anchor");
  ASSERT_TRUE(anchor_node);
  const auto& attributes = *anchor_node->content_attributes;
  EXPECT_EQ(attributes.attribute_type,
            mojom::blink::AIPageContentAttributeType::kAnchor);
  ASSERT_TRUE(attributes.geometry);

  const auto& geometry = *attributes.geometry;
  EXPECT_EQ(gfx::Rect(40, 60, 102, 102), geometry.outer_bounding_box);
  EXPECT_EQ(geometry.outer_bounding_box, geometry.visible_bounding_box);
}

TEST_F(AIPageContentAgentTest, TableTextClippedByScrollerBeforeScroll) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <style>
        body { margin: 0; font: 10px/10px Ahem; }
        * { margin: 0; padding: 0; box-sizing: border-box; }
        main {
          width: 70px;
          height: 30px;
          overflow: auto;
        }
        table {
          width: 120%;
          height: 30px;
          table-layout: fixed;
          border-collapse: collapse;
        }
        td.small10x10 {
          width: 100px;
          height: 3ch;
          vertical-align: top;
          font: 10px/10px Ahem;
        }
      </style>
      <body>
        <main id="scroller">
          <table>
            <tr>
              <td id="cell" class="small10x10">ABC DEF GHI JKL MNO PQR STU VWX YZ 0123456789</td>
              <td></td>
            </tr>
            <tr>
              <td></td>
              <td style="height: 100vh; width: 100vh;"></td>
            </tr>
          </table>
        </main>
      </body>)HTML",
      url_test_helpers::ToKURL("http://example.com"));

  LoadAhem();

  GetAIPageContentWithActionableElements();

  Document* document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
  ASSERT_TRUE(document);

  Element* cell = document->getElementById(AtomicString("cell"));
  ASSERT_TRUE(cell);
  Node* text = cell->firstChild();
  ASSERT_TRUE(text);
  ASSERT_TRUE(text->IsTextNode());

  const auto* text_node = FindNodeByDomNodeId(DOMNodeIds::IdForNode(text));
  ASSERT_TRUE(text_node);
  ASSERT_TRUE(text_node->content_attributes->geometry);

  Element* scroller_element =
      document->getElementById(AtomicString("scroller"));
  ASSERT_TRUE(scroller_element);

  const auto& geometry = *text_node->content_attributes->geometry;
  const auto& fragments = geometry.fragment_visible_bounding_boxes;
  const ComputedStyle& before_style = text->GetLayoutObject()->StyleRef();
  LocalFrame* local_frame = document->GetFrame();
  ASSERT_TRUE(local_frame);
  EXPECT_NEAR(before_style.ComputedFontSize(), 10.0f, 0.01f);
  EXPECT_NEAR(before_style.GetFont()->GetFontDescription().ComputedSize(),
              10.0f, 0.01f);

  EXPECT_EQ(gfx::Rect(0, 0, 100, 50), geometry.outer_bounding_box);
  EXPECT_EQ(gfx::Rect(0, 0, 70, 30), geometry.visible_bounding_box);
  ASSERT_EQ(fragments.size(), 3u);
  EXPECT_EQ(fragments[0], gfx::Rect(0, 0, 70, 10));
  EXPECT_EQ(fragments[1], gfx::Rect(0, 10, 70, 10));
  EXPECT_EQ(fragments[2], gfx::Rect(0, 20, 70, 10));
}

TEST_F(AIPageContentAgentTest, TableTextClippedByScrollerAfterScroll) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <style>
        body { margin: 0; font: 10px/10px Ahem; }
        * { margin: 0; padding: 0; box-sizing: border-box; }
        main {
          width: 70px;
          height: 30px;
          overflow: auto;
        }
        table {
          width: 120%;
          height: 30px;
          table-layout: fixed;
          border-collapse: collapse;
        }
        td.small10x10 {
          width: 100px;
          height: 3ch;
          vertical-align: top;
          font: 10px/10px Ahem;
        }
      </style>
      <body>
        <main id="scroller">
          <table>
            <tr>
              <td id="cell" class="small10x10">ABC DEF GHI JKL MNO PQR STU VWX YZ 0123456789</td>
              <td></td>
            </tr>
            <tr>
              <td></td>
              <td style="height: 100vh; width: 100vh;"></td>
            </tr>
          </table>
        </main>
      </body>)HTML",
      url_test_helpers::ToKURL("http://example.com"));

  LoadAhem();

  Document* document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
  ASSERT_TRUE(document);

  Element* scroller = document->getElementById(AtomicString("scroller"));
  ASSERT_TRUE(scroller);
  scroller->setScrollLeft(15);
  scroller->setScrollTop(15);

  LocalFrameView* view = document->View();
  ASSERT_TRUE(view);
  test::RunPendingTasks();
  view->UpdateAllLifecyclePhasesForTest();

  GetAIPageContentWithActionableElements();

  Element* cell = document->getElementById(AtomicString("cell"));
  ASSERT_TRUE(cell);
  Node* text = cell->firstChild();
  ASSERT_TRUE(text);
  ASSERT_TRUE(text->IsTextNode());

  const auto* text_node = FindNodeByDomNodeId(DOMNodeIds::IdForNode(text));
  ASSERT_TRUE(text_node);
  ASSERT_TRUE(text_node->content_attributes->geometry);

  const auto& geometry = *text_node->content_attributes->geometry;
  const auto& fragments = geometry.fragment_visible_bounding_boxes;
  const ComputedStyle& after_style = text->GetLayoutObject()->StyleRef();
  std::string fragments_trace;
  for (size_t i = 0; i < fragments.size(); ++i) {
    if (!fragments_trace.empty()) {
      fragments_trace.append("; ");
    }
    std::string fragment_rect = fragments[i].ToString();
    fragments_trace.append(
        base::StringPrintf("%zu:%s", i, fragment_rect.c_str()));
  }
  LocalFrame* local_frame = document->GetFrame();
  ASSERT_TRUE(local_frame);
  EXPECT_NEAR(after_style.ComputedFontSize(), 10.0f, 0.01f);
  EXPECT_NEAR(after_style.GetFont()->GetFontDescription().ComputedSize(), 10.0f,
              0.01f);

  EXPECT_EQ(gfx::Rect(-15, -15, 100, 50), geometry.outer_bounding_box);
  EXPECT_EQ(gfx::Rect(0, 0, 70, 30), geometry.visible_bounding_box);
  ASSERT_EQ(fragments.size(), 4u);
  EXPECT_EQ(fragments[0], gfx::Rect(0, 0, 55, 5));
  EXPECT_EQ(fragments[1], gfx::Rect(0, 5, 55, 10));
  EXPECT_EQ(fragments[2], gfx::Rect(0, 15, 70, 10));
  EXPECT_EQ(fragments[3], gfx::Rect(0, 25, 70, 5));
}

TEST_F(AIPageContentAgentTest, IframeInlineTextClippedWhenViewportScrolled) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <style>
        body { margin: 0; height: 2000px; font: 10px/10px Ahem; }
        iframe {
          border: none;
          width: 100px;
          height: 200px;
        }
      </style>
      <body>
        <iframe id="target" srcdoc="
          <style>
            body { margin: 0; font: 10px/10px Ahem; }
            p { margin: 0; background: red; }
          </style>
          <body>
            <p id='inner'>ABC DEF GHI JKL MNO PQR STU</p>
          </body>">
        </iframe>
      </body>)HTML",
      url_test_helpers::ToKURL("http://example.com"));

  LoadAhem();

  Document* document = helper_.LocalMainFrame()->GetFrame()->GetDocument();
  ASSERT_TRUE(document);
  Element* scrolling_element = document->scrollingElement();
  ASSERT_TRUE(scrolling_element);
  scrolling_element->setScrollLeft(15);
  scrolling_element->setScrollTop(15);

  LocalFrameView* view = document->View();
  ASSERT_TRUE(view);
  test::RunPendingTasks();
  view->UpdateAllLifecyclePhasesForTest();

  HTMLIFrameElement* iframe_element = DynamicTo<HTMLIFrameElement>(
      document->getElementById(AtomicString("target")));
  ASSERT_TRUE(iframe_element);
  LocalFrame* child_frame =
      DynamicTo<LocalFrame>(iframe_element->ContentFrame());
  ASSERT_TRUE(child_frame);
  // Load Ahem inside the iframe before we snapshot geometry to avoid races on
  // platforms where font activation is asynchronous.
  PageTestBase::LoadAhem(*child_frame);
  test::RunPendingTasks();
  if (LocalFrameView* child_view = child_frame->View()) {
    child_view->UpdateAllLifecyclePhasesForTest();
  }
  Document* child_document = child_frame->GetDocument();
  ASSERT_TRUE(child_document);

  Element* inner_p = child_document->getElementById(AtomicString("inner"));
  ASSERT_TRUE(inner_p);
  Node* inner_text = inner_p->firstChild();
  ASSERT_TRUE(inner_text);
  ASSERT_TRUE(inner_text->IsTextNode());

  GetAIPageContentWithActionableElements();

  const auto* text_node =
      FindNodeByDomNodeId(DOMNodeIds::IdForNode(inner_text));
  ASSERT_TRUE(text_node);
  ASSERT_TRUE(text_node->content_attributes->geometry);

  const auto& geometry = *text_node->content_attributes->geometry;
  const auto& fragments = geometry.fragment_visible_bounding_boxes;
  const ComputedStyle& iframe_style = inner_text->GetLayoutObject()->StyleRef();
  std::string fragments_trace;
  for (size_t i = 0; i < fragments.size(); ++i) {
    if (!fragments_trace.empty()) {
      fragments_trace.append("; ");
    }
    std::string fragment_rect = fragments[i].ToString();
    fragments_trace.append(
        base::StringPrintf("%zu:%s", i, fragment_rect.c_str()));
  }
  EXPECT_NEAR(iframe_style.ComputedFontSize(), 10.0f, 0.01f);
  EXPECT_NEAR(iframe_style.GetFont()->GetFontDescription().ComputedSize(),
              10.0f, 0.01f);
  EXPECT_EQ(
      AtomicString("Ahem"),
      iframe_style.GetFont()->GetFontDescription().FirstFamily().FamilyName());

  EXPECT_EQ(gfx::Rect(0, -15, 70, 40), geometry.outer_bounding_box);
  EXPECT_EQ(gfx::Rect(0, 0, 70, 25), geometry.visible_bounding_box);
  ASSERT_EQ(fragments.size(), 3u);
  EXPECT_EQ(fragments[0], gfx::Rect(0, 0, 70, 5));
  EXPECT_EQ(fragments[1], gfx::Rect(0, 5, 70, 10));
  EXPECT_EQ(fragments[2], gfx::Rect(0, 15, 30, 10));
}

TEST_F(AIPageContentAgentTest, CSSHoverPseudoClass) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"(
      <body>
        <style>
          div:hover {
            background-color: red;
          }
        </style>
        <div onclick=console.log(1)></div>
        <p>sibling</p>
      </body>)",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];
  EXPECT_TRUE(
      div_node.content_attributes->node_interaction_info->clickability_reasons
          .Contains(ClickabilityReason::kHoverPseudoClass));

  const auto& p_node = *ContentRootNode().children_nodes[1];
  EXPECT_FALSE(
      p_node.content_attributes->node_interaction_info->clickability_reasons
          .Contains(ClickabilityReason::kHoverPseudoClass));
}

TEST_F(AIPageContentAgentTest, CSSHoverPseudoClassNotInherited) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(), R"(
      <body>
        <style>
          div:hover {
            background-color: red;
          }
        </style>
        <div onclick=console.log(1)>
          <p>child</p>
        </div>
        <p>sibling</p>
      </body>)",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& div_node = *ContentRootNode().children_nodes[0];
  EXPECT_TRUE(
      div_node.content_attributes->node_interaction_info->clickability_reasons
          .Contains(ClickabilityReason::kHoverPseudoClass));

  const auto& child_p_node = *div_node.children_nodes[0];
  EXPECT_FALSE(child_p_node.content_attributes->node_interaction_info
                   ->clickability_reasons.Contains(
                       ClickabilityReason::kHoverPseudoClass));

  const auto& sibling_p_node = *ContentRootNode().children_nodes[1];
  EXPECT_FALSE(sibling_p_node.content_attributes->node_interaction_info
                   ->clickability_reasons.Contains(
                       ClickabilityReason::kHoverPseudoClass));
}

// Tests hit-testing and z-order computations for AIPageContentAgent.
class AIPageContentAgentTestZOrder : public base::test::WithFeatureOverride,
                                     public AIPageContentAgentTest {
 public:
  AIPageContentAgentTestZOrder()
      : base::test::WithFeatureOverride(
            blink::features::kAIPageContentZOrderEarlyFiltering) {}
  ~AIPageContentAgentTestZOrder() override = default;
};

TEST_P(AIPageContentAgentTestZOrder, HitTestElementsBasic) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <p style='background:red'>Text 1</p>"
      "  <p>Text 2</p>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

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

  if (IsParamFeatureEnabled()) {
    // When relying directly on the hit test result for z order, the tree should
    // look as follows, with the given z order. This is consistent with "tree
    // order" depth-first traversal as defined in the CSS spec:
    // https://www.w3.org/TR/CSS2/zindex.html
    // root - 1
    // |_html - 2
    //    |_body - 3
    //      |_p - 4
    //      | |_Text1 - 5
    //      |_p - 6
    //        |_Text2 - 7
    ASSERT_EQ(body.children_nodes.size(), 2u);
    const auto& p1 = *body.children_nodes.at(0);
    EXPECT_EQ(
        p1.content_attributes->node_interaction_info->document_scoped_z_order,
        4);

    ASSERT_EQ(p1.children_nodes.size(), 1u);
    const auto& text1 = *p1.children_nodes.at(0);
    CheckTextNode(text1, "Text 1");
    EXPECT_EQ(text1.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              5);

    const auto& p2 = *body.children_nodes.at(1);
    EXPECT_EQ(
        p2.content_attributes->node_interaction_info->document_scoped_z_order,
        6);

    ASSERT_EQ(p2.children_nodes.size(), 1u);
    const auto& text2 = *p2.children_nodes.at(0);
    CheckTextNode(text2, "Text 2");
    EXPECT_EQ(text2.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              7);
  } else {
    // The tree should look as follows, with the given z order.
    // root - 1
    // |_html - 2
    //    |_body - 3
    //      |_p - 4
    //      | |_Text1 - 6
    //      |_p - 5
    //        |_Text2 - 7
    ASSERT_EQ(body.children_nodes.size(), 2u);
    const auto& p1 = *body.children_nodes.at(0);
    EXPECT_EQ(
        p1.content_attributes->node_interaction_info->document_scoped_z_order,
        4);

    const auto& p2 = *body.children_nodes.at(1);
    EXPECT_EQ(
        p2.content_attributes->node_interaction_info->document_scoped_z_order,
        5);

    ASSERT_EQ(p1.children_nodes.size(), 1u);
    const auto& text1 = *p1.children_nodes.at(0);
    CheckTextNode(text1, "Text 1");
    EXPECT_EQ(text1.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              6);

    ASSERT_EQ(p2.children_nodes.size(), 1u);
    const auto& text2 = *p2.children_nodes.at(0);
    CheckTextNode(text2, "Text 2");
    EXPECT_EQ(text2.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              7);
  }
}

TEST_P(AIPageContentAgentTestZOrder, HitTestElementsFixedPos) {
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

TEST_P(AIPageContentAgentTestZOrder, HitTestElementsPointerNone) {
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

TEST_P(AIPageContentAgentTestZOrder, HitTestElementsOffscreen) {
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
  EXPECT_FALSE(interaction_info.clickability_reasons.empty());
  EXPECT_FALSE(interaction_info.document_scoped_z_order);
}

TEST_P(AIPageContentAgentTestZOrder, HitTestElementsIframe) {
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

  if (IsParamFeatureEnabled()) {
    // If we're respecting tree-order traversal when computing z-order, the
    // iframe will have a lower z-order because it is the first child.
    EXPECT_LT(
        *iframe.content_attributes->node_interaction_info
             ->document_scoped_z_order,
        *p.content_attributes->node_interaction_info->document_scoped_z_order);
  } else {
    EXPECT_GT(
        *iframe.content_attributes->node_interaction_info
             ->document_scoped_z_order,
        *p.content_attributes->node_interaction_info->document_scoped_z_order);
  }

  ASSERT_EQ(iframe.children_nodes.size(), 1u);
  const auto& doc_inside_iframe = *iframe.children_nodes.at(0);
  ASSERT_TRUE(doc_inside_iframe.content_attributes->node_interaction_info);
  ASSERT_TRUE(doc_inside_iframe.content_attributes->node_interaction_info
                  ->document_scoped_z_order);
  EXPECT_EQ(*doc_inside_iframe.content_attributes->node_interaction_info
                 ->document_scoped_z_order,
            1);
}

TEST_P(AIPageContentAgentTestZOrder, OverflowHiddenGeometry) {
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

TEST_P(AIPageContentAgentTestZOrder, OverflowVisibleGeometry) {
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

TEST_P(AIPageContentAgentTestZOrder, HitTestElementsRelativePos) {
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

TEST_P(AIPageContentAgentTestZOrder, LabelWithText) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body style='margin:0; font:1px/1px Ahem'>"
      "  <label>xyz</label>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  LoadAhem();

  GetAIPageContentWithActionableElements();

  const auto& body = ContentRootNode();
  ASSERT_EQ(body.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kContainer);
  const auto& label = *body.children_nodes.at(0);
  ASSERT_EQ(label.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kContainer);
  const auto& text = *label.children_nodes.at(0);
  ASSERT_EQ(text.content_attributes->attribute_type,
            mojom::blink::AIPageContentAttributeType::kText);

  SCOPED_TRACE("Label");
  CheckGeometry(label, gfx::Rect(0, 0, 3, 1), gfx::Rect(0, 0, 3, 1));
  SCOPED_TRACE("Text");
  CheckGeometry(text, gfx::Rect(0, 0, 3, 1), gfx::Rect(0, 0, 3, 1));
}

TEST_P(AIPageContentAgentTestZOrder, HitTestElementsAnchorWithSpanParent) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
      <body>
        <span id="target-span">
          <a id="link" href="https://example.com">
            <span id="inner-span">
              This is the inner span.
             </span>
            <div id="inner-div">
              This is the inner div.
            </div>
          </a>
        </span>
      </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = *Content()->root_node;
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
  ASSERT_EQ(body.children_nodes.size(), 1u);

  if (IsParamFeatureEnabled()) {
    // When filtering for duplicate hit test nodes early, the resulting z-order
    // will follow "tree order", which is depth-first. The resulting tree with
    // corresponding z-order will be:
    // root - 1
    // |_html - 2
    //    |_body - 3
    //      |_span - 4
    //        |_a - 5
    //          |_span - 6
    //          | |_text - 7
    //          |_div - 8
    //            |_text - 9

    const auto& target_span = *body.children_nodes.at(0);
    EXPECT_EQ(target_span.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              4);
    ASSERT_EQ(target_span.children_nodes.size(), 1u);

    const auto& anchor = *target_span.children_nodes.at(0);
    EXPECT_EQ(anchor.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              5);
    ASSERT_EQ(anchor.children_nodes.size(), 2u);

    const auto& inner_span = *anchor.children_nodes.at(0);
    EXPECT_EQ(inner_span.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              6);
    ASSERT_EQ(inner_span.children_nodes.size(), 1u);

    const auto& span_text = *inner_span.children_nodes.at(0);
    EXPECT_EQ(span_text.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              7);

    const auto& inner_div = *anchor.children_nodes.at(1);
    EXPECT_EQ(inner_div.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              8);
    ASSERT_EQ(inner_div.children_nodes.size(), 1u);

    const auto& div_text = *inner_div.children_nodes.at(0);
    EXPECT_EQ(div_text.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              9);
  } else {
    // When we don't filter for duplicate hit test nodes early, the span
    // containers will be included multiple times, causing tree order to be
    // violated. The resulting tree with corresponding z-order will be:
    // root - 1
    // |_html - 2
    //    |_body - 3
    //      |_span - 6
    //        |_a - 4
    //          |_span - 7
    //          | |_text - 8
    //          |_div - 5
    //            |_text - 9

    const auto& target_span = *body.children_nodes.at(0);
    EXPECT_EQ(target_span.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              6);
    ASSERT_EQ(target_span.children_nodes.size(), 1u);

    const auto& anchor = *target_span.children_nodes.at(0);
    EXPECT_EQ(anchor.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              4);
    ASSERT_EQ(anchor.children_nodes.size(), 2u);

    const auto& inner_span = *anchor.children_nodes.at(0);
    EXPECT_EQ(inner_span.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              7);
    ASSERT_EQ(inner_span.children_nodes.size(), 1u);

    const auto& span_text = *inner_span.children_nodes.at(0);
    EXPECT_EQ(span_text.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              8);

    const auto& inner_div = *anchor.children_nodes.at(1);
    EXPECT_EQ(inner_div.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              5);
    ASSERT_EQ(inner_div.children_nodes.size(), 1u);

    const auto& div_text = *inner_div.children_nodes.at(0);
    EXPECT_EQ(div_text.content_attributes->node_interaction_info
                  ->document_scoped_z_order,
              9);
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(AIPageContentAgentTestZOrder);

TEST_F(AIPageContentAgentTest, LinkWithOverflowGeometry) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body>"
      "  <style>"
      "    a {"
      "      display: inline-block;"
      "      width: 20px;"
      "      height: 20px;"
      "      overflow: visible;"
      "    }"
      "  </style>"
      "  <a href='#' style='font-size: 50px'>Text that will overflow</a>"
      "</body>",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  EXPECT_EQ(root.children_nodes.size(), 1u);

  auto& anchor_node = *root.children_nodes[0];
  CheckAnchorNode(anchor_node, url_test_helpers::ToKURL("http://foobar.com/#"),
                  {});

  const auto& geometry = *anchor_node.content_attributes->geometry;
  // Visible overflow is not currently included in the outer_bounding_box or
  // visible_bounding_box. This may not be the correct choice given that visible
  // overflow is hit testable. However, this is not a major concern because
  // events bubble up. If we see a problem case caused by the lack of overflow
  // inclusion we can attempt to incorporate hit test rects (which was attempted
  // previously but was non-trivial).
  EXPECT_EQ(geometry.outer_bounding_box.width(), 20);
  EXPECT_EQ(geometry.outer_bounding_box.width(), 20);
  EXPECT_EQ(geometry.visible_bounding_box.width(), 20);
  EXPECT_EQ(geometry.visible_bounding_box.height(), 20);
}

TEST_F(AIPageContentAgentTest, CursorNotAllowedButton) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
        <body>
         <button style="cursor: not-allowed;">Text</button>
        </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);
  const auto& button = *root.children_nodes.at(0);
  CheckHitTestableAndInteractive(button,
                                 {ClickabilityReason::kClickableControl});
  EXPECT_FALSE(button.content_attributes->node_interaction_info->is_disabled);
  EXPECT_THAT(button.content_attributes->node_interaction_info
                  ->interaction_disabled_reasons,
              testing::UnorderedElementsAre(
                  InteractionDisabledReason::kCursorNotAllowed));
}

TEST_F(AIPageContentAgentTest, MultipleInteractionDisabledReasons) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      R"HTML(
        <body>
         <button disabled aria-disabled=true style="cursor: not-allowed;">
           Text
         </button>
        </body>
      )HTML",
      url_test_helpers::ToKURL("http://foobar.com"));

  GetAIPageContentWithActionableElements();

  const auto& root = ContentRootNode();
  ASSERT_EQ(root.children_nodes.size(), 1u);
  const auto& button = *root.children_nodes.at(0);
  CheckHitTestableButNotInteractive(button);
  EXPECT_TRUE(button.content_attributes->node_interaction_info->is_disabled);
  EXPECT_THAT(button.content_attributes->node_interaction_info
                  ->interaction_disabled_reasons,
              testing::UnorderedElementsAre(
                  InteractionDisabledReason::kDisabled,
                  InteractionDisabledReason::kAriaDisabled,
                  InteractionDisabledReason::kCursorNotAllowed));
}

}  // namespace
}  // namespace blink
