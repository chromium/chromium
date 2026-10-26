// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/annotated_page_content_extraction_utils.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/test/values_test_util.h"
#import "base/values.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/frame_grafter.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/gfx/geometry/rect.h"
#import "url/gurl.h"
#import "url/origin.h"

using AnnotatedPageContentExtractionUtilsTest = PlatformTest;

namespace {

// Returns `rect` as a `gfx::Rect`.
gfx::Rect ToGfxRect(const optimization_guide::proto::BoundingRect& rect) {
  return gfx::Rect(rect.x(), rect.y(), rect.width(), rect.height());
}

// Copies `source` into `destination`.
void SetBoundingRect(const gfx::Rect& source,
                     optimization_guide::proto::BoundingRect* destination) {
  destination->set_x(source.x());
  destination->set_y(source.y());
  destination->set_width(source.width());
  destination->set_height(source.height());
}

// Appends a child of `type` to `parent` with an `outer` box and, if set, a
// `visible` box.
optimization_guide::proto::ContentNode* AddChildWithGeometry(
    optimization_guide::proto::ContentNode* parent,
    optimization_guide::proto::ContentAttributeType type,
    const gfx::Rect& outer,
    const std::optional<gfx::Rect>& visible) {
  optimization_guide::proto::ContentNode* child = parent->add_children_nodes();
  optimization_guide::proto::ContentAttributes* attributes =
      child->mutable_content_attributes();
  attributes->set_attribute_type(type);
  optimization_guide::proto::Geometry* geometry =
      attributes->mutable_geometry();
  SetBoundingRect(outer, geometry->mutable_outer_bounding_box());
  if (visible) {
    SetBoundingRect(*visible, geometry->mutable_visible_bounding_box());
  }
  return child;
}

}  // namespace

// Tests that PopulateAPCNodeFromContentTree does not populate a rectangle
// if one of its components is missing.
TEST_F(AnnotatedPageContentExtractionUtilsTest, IncompleteRectangleIgnored) {
  optimization_guide::proto::ContentNode node;
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  // Dummy grafter.
  FrameGrafter grafter;

  // 1. Missing 'height' in outerBoundingBox.
  // 2. Complete visibleBoundingBox.
  // Note: 'attributeType' is mandatory for population to proceed.
  base::Value node_content = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 1,
        "geometry": {
          "outerBoundingBox": {
            "x": 10,
            "y": 20,
            "width": 100
          },
          "visibleBoundingBox": {
            "x": 15,
            "y": 25,
            "width": 50,
            "height": 60
          }
        }
      }
    }
  )");

  ASSERT_TRUE(node_content.is_dict());
  base::flat_map<std::string, uint32_t> section_numbers;
  AutofillExtractionContext context(nullptr, std::nullopt, false, false,
                                    &section_numbers);
  PopulateAPCNodeFromContentTree(
      node_content.GetDict(), origin, grafter, &context, &node,
      base::BindRepeating(
          [](bool is_focused, const std::string& document_id) {}));

  ASSERT_TRUE(node.has_content_attributes());
  ASSERT_TRUE(node.content_attributes().has_geometry());

  // Outer box should NOT be set because it's missing height.
  EXPECT_FALSE(node.content_attributes().geometry().has_outer_bounding_box());

  // Visible box SHOULD be set because it's complete.
  EXPECT_TRUE(node.content_attributes().geometry().has_visible_bounding_box());
  EXPECT_EQ(node.content_attributes().geometry().visible_bounding_box().x(),
            15);
  EXPECT_EQ(
      node.content_attributes().geometry().visible_bounding_box().height(), 60);
}

// Tests that PopulateAPCNodeFromContentTree handles redactedFrameMetadata.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       RedactedFrameMetadataPopulated) {
  optimization_guide::proto::ContentNode node;
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  FrameGrafter grafter;

  base::Value node_content = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 3,
        "iframeData": {
          "content": {
            "redactedFrameMetadata": {
              "reason": 1
            }
          }
        }
      }
    }
  )");

  ASSERT_TRUE(node_content.is_dict());
  base::flat_map<std::string, uint32_t> section_numbers;
  AutofillExtractionContext context(nullptr, std::nullopt, false, false,
                                    &section_numbers);
  PopulateAPCNodeFromContentTree(
      node_content.GetDict(), origin, grafter, &context, &node,
      base::BindRepeating(
          [](bool is_focused, const std::string& document_id) {}));

  ASSERT_TRUE(node.has_content_attributes());
  EXPECT_EQ(node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  EXPECT_TRUE(node.content_attributes().has_iframe_data());
  EXPECT_TRUE(
      node.content_attributes().iframe_data().has_redacted_frame_metadata());
  EXPECT_EQ(node.content_attributes()
                .iframe_data()
                .redacted_frame_metadata()
                .reason(),
            optimization_guide::proto::
                IframeData_RedactedFrameMetadata_Reason_REASON_CROSS_SITE);

  optimization_guide::proto::ContentNode node_cross_origin;
  base::Value node_content_cross_origin = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 3,
        "iframeData": {
          "content": {
            "redactedFrameMetadata": {
              "reason": 2
            }
          }
        }
      }
    }
  )");
  PopulateAPCNodeFromContentTree(
      node_content_cross_origin.GetDict(), origin, grafter, &context,
      &node_cross_origin,
      base::BindRepeating(
          [](bool is_focused, const std::string& document_id) {}));

  EXPECT_EQ(node_cross_origin.content_attributes()
                .iframe_data()
                .redacted_frame_metadata()
                .reason(),
            optimization_guide::proto::
                IframeData_RedactedFrameMetadata_Reason_REASON_CROSS_ORIGIN);
}

// Tests that PopulateAPCNodeFromContentTree handles a completely empty
// geometry.
TEST_F(AnnotatedPageContentExtractionUtilsTest, EmptyGeometryIgnored) {
  optimization_guide::proto::ContentNode node;
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  FrameGrafter grafter;

  // Note: 'attributeType' is mandatory for population to proceed.
  base::Value node_content = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 1,
        "geometry": {}
      }
    }
  )");

  ASSERT_TRUE(node_content.is_dict());
  base::flat_map<std::string, uint32_t> section_numbers;
  AutofillExtractionContext context(nullptr, std::nullopt, false, false,
                                    &section_numbers);
  PopulateAPCNodeFromContentTree(
      node_content.GetDict(), origin, grafter, &context, &node,
      base::BindRepeating(
          [](bool is_focused, const std::string& document_id) {}));

  ASSERT_TRUE(node.has_content_attributes());
  EXPECT_FALSE(node.content_attributes().has_geometry());
}

// Tests that ResolveCrossSiteFrameContent redacts placeholders that are left
// unresolved and are cross-site when `include_same_site_only` is true.
// Same-site cross-origin placeholders are not redacted when
// `include_same_site_only` is true.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       UnresolvedPlaceholdersRedacted) {
  web::FakeWebState web_state;
  web_state.SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  web_state.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  autofill::ChildFrameRegistrar::CreateForWebState(&web_state);
  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::FromWebState(&web_state);

  FrameGrafter grafter;
  autofill::RemoteFrameToken same_origin_token(
      base::UnguessableToken::Create());
  autofill::RemoteFrameToken same_site_token(base::UnguessableToken::Create());
  autofill::RemoteFrameToken cross_site_token(base::UnguessableToken::Create());

  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_url("https://example.com");

  // Create same-origin placeholder (not grafted).
  optimization_guide::proto::ContentNode* same_origin_placeholder =
      apc.mutable_root_node()->add_children_nodes();
  same_origin_placeholder->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  auto* same_origin_frame_data =
      same_origin_placeholder->mutable_content_attributes()
          ->mutable_iframe_data()
          ->mutable_frame_data();
  same_origin_frame_data->set_url("https://example.com/same-origin");
  auto* same_origin_security_origin =
      same_origin_frame_data->mutable_security_origin();
  same_origin_security_origin->set_opaque(false);
  same_origin_security_origin->set_value("https://example.com/same-origin");
  grafter.RegisterPlaceholder(same_origin_token, same_origin_placeholder);

  // Create same-site cross-origin placeholder (not grafted).
  optimization_guide::proto::ContentNode* same_site_placeholder =
      apc.mutable_root_node()->add_children_nodes();
  same_site_placeholder->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  auto* same_site_frame_data =
      same_site_placeholder->mutable_content_attributes()
          ->mutable_iframe_data()
          ->mutable_frame_data();
  same_site_frame_data->set_url("https://sub.example.com/same-site");
  auto* same_site_security_origin =
      same_site_frame_data->mutable_security_origin();
  same_site_security_origin->set_opaque(false);
  same_site_security_origin->set_value("https://sub.example.com/same-site");
  grafter.RegisterPlaceholder(same_site_token, same_site_placeholder);

  // Create cross-site placeholder (not grafted).
  optimization_guide::proto::ContentNode* cross_site_placeholder =
      apc.mutable_root_node()->add_children_nodes();
  cross_site_placeholder->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  auto* cross_site_frame_data =
      cross_site_placeholder->mutable_content_attributes()
          ->mutable_iframe_data()
          ->mutable_frame_data();
  cross_site_frame_data->set_url("https://different-domain.com/cross-site");
  auto* cross_site_security_origin =
      cross_site_frame_data->mutable_security_origin();
  cross_site_security_origin->set_opaque(false);
  cross_site_security_origin->set_value(
      "https://different-domain.com/cross-site");
  grafter.RegisterPlaceholder(cross_site_token, cross_site_placeholder);

  ResolveCrossSiteFrameContent(
      grafter, registrar, /*include_same_site_only=*/true, &apc,
      web_state.GetWebFramesManager(web::ContentWorld::kPageContentWorld));

  // Same-origin placeholder should not be redacted.
  EXPECT_TRUE(same_origin_placeholder->content_attributes()
                  .iframe_data()
                  .has_frame_data());
  EXPECT_FALSE(same_origin_placeholder->content_attributes()
                   .iframe_data()
                   .has_redacted_frame_metadata());

  // Same-site cross-origin placeholder should not be redacted.
  EXPECT_TRUE(same_site_placeholder->content_attributes()
                  .iframe_data()
                  .has_frame_data());
  EXPECT_FALSE(same_site_placeholder->content_attributes()
                   .iframe_data()
                   .has_redacted_frame_metadata());

  // Cross-site placeholder should be redacted.
  EXPECT_FALSE(cross_site_placeholder->content_attributes()
                   .iframe_data()
                   .has_frame_data());
  EXPECT_TRUE(cross_site_placeholder->content_attributes()
                  .iframe_data()
                  .has_redacted_frame_metadata());
  EXPECT_EQ(cross_site_placeholder->content_attributes()
                .iframe_data()
                .redacted_frame_metadata()
                .reason(),
            optimization_guide::proto::
                IframeData_RedactedFrameMetadata_Reason_REASON_CROSS_SITE);
}

// Tests that when include_same_site_only is false, unresolved placeholders are
// NOT stamped with REASON_CROSS_SITE redaction metadata.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       ResolveCrossSiteFrameContent_NoRedactionWhenSameSiteDisabled) {
  web::FakeWebState web_state;
  web_state.SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  web_state.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  autofill::ChildFrameRegistrar::CreateForWebState(&web_state);
  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::FromWebState(&web_state);
  FrameGrafter grafter;
  autofill::RemoteFrameToken cross_site_token =
      autofill::RemoteFrameToken(base::UnguessableToken::Create());

  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_url("https://example.com");

  optimization_guide::proto::ContentNode* cross_site_placeholder =
      apc.mutable_root_node()->add_children_nodes();
  cross_site_placeholder->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  auto* cross_site_frame_data =
      cross_site_placeholder->mutable_content_attributes()
          ->mutable_iframe_data()
          ->mutable_frame_data();
  cross_site_frame_data->set_url("https://different-domain.com/cross-site");
  auto* cross_site_security_origin =
      cross_site_frame_data->mutable_security_origin();
  cross_site_security_origin->set_opaque(false);
  cross_site_security_origin->set_value(
      "https://different-domain.com/cross-site");
  grafter.RegisterPlaceholder(cross_site_token, cross_site_placeholder);

  ResolveCrossSiteFrameContent(
      grafter, registrar, /*include_same_site_only=*/false, &apc,
      web_state.GetWebFramesManager(web::ContentWorld::kPageContentWorld));

  // Cross-site placeholder should NOT be redacted when same-site gating is off.
  EXPECT_TRUE(cross_site_placeholder->content_attributes()
                  .iframe_data()
                  .has_frame_data());
  EXPECT_FALSE(cross_site_placeholder->content_attributes()
                   .iframe_data()
                   .has_redacted_frame_metadata());
}

TEST_F(AnnotatedPageContentExtractionUtilsTest,
       IframePlaceholderPreservesGeometry) {
  base::Value node_content = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 5,
        "iframeData": {
          "remoteFrameToken": {
            "value": "1234567890ABCDEF1234567890ABCDEF"
          }
        },
        "geometry": {
          "visibleBoundingBox": {
            "x": 20,
            "y": 350,
            "width": 375,
            "height": 500
          }
        }
      }
    }
  )");

  ASSERT_TRUE(node_content.is_dict());
  FrameGrafter grafter;
  optimization_guide::proto::ContentNode destination_node;
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  PopulateAPCNodeFromContentTree(
      node_content.GetDict(), origin, grafter,
      /*autofill_context=*/nullptr, &destination_node,
      base::RepeatingCallback<void(bool, const std::string&)>());

  EXPECT_TRUE(destination_node.content_attributes().has_geometry());
  EXPECT_TRUE(destination_node.content_attributes()
                  .geometry()
                  .has_visible_bounding_box());
  EXPECT_EQ(destination_node.content_attributes()
                .geometry()
                .visible_bounding_box()
                .x(),
            20);
  EXPECT_EQ(destination_node.content_attributes()
                .geometry()
                .visible_bounding_box()
                .y(),
            350);
  EXPECT_EQ(destination_node.content_attributes()
                .geometry()
                .visible_bounding_box()
                .width(),
            375);
  EXPECT_EQ(destination_node.content_attributes()
                .geometry()
                .visible_bounding_box()
                .height(),
            500);
}

// Tests that un-grafted / orphan subframes (frames extracted in the background
// that do not correspond to any placeholder in the main DOM tree) are dropped
// without being appended to the root node.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       ResolveCrossSiteFrameContent_UnregisteredOrphanFramesDropped) {
  web::FakeWebState web_state;
  web_state.SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  web_state.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  autofill::ChildFrameRegistrar::CreateForWebState(&web_state);
  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::FromWebState(&web_state);
  FrameGrafter grafter;

  // Declare an orphan subframe (e.g. invisible tracking iframe) in the grafter.
  autofill::LocalFrameToken orphan_token =
      autofill::LocalFrameToken(base::UnguessableToken::Create());
  FrameGrafter::FrameContent* orphan_content =
      grafter.DeclareContent(orphan_token);
  orphan_content->content.mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
  orphan_content->content.mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content("Orphan Frame Content");
  orphan_content->frame_data.set_url("https://tracker.example.com/sync");

  // Setup the main APC tree with a root node and 1 child paragraph.
  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_url("https://example.com");
  optimization_guide::proto::ContentNode* root_node = apc.mutable_root_node();
  root_node->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  optimization_guide::proto::ContentNode* child_node =
      root_node->add_children_nodes();
  child_node->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  child_node->mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content("Main Frame Paragraph");

  ASSERT_EQ(root_node->children_nodes_size(), 1);

  // Run resolution.
  ResolveCrossSiteFrameContent(
      grafter, registrar, /*include_same_site_only=*/false, &apc,
      web_state.GetWebFramesManager(web::ContentWorld::kPageContentWorld));

  // The orphan frame should NOT be appended to root_node.
  EXPECT_EQ(root_node->children_nodes_size(), 1);
  EXPECT_EQ(root_node->children_nodes(0)
                .content_attributes()
                .text_data()
                .text_content(),
            "Main Frame Paragraph");
}

// Tests that dropping an unregistered orphan frame that contains a nested
// cross-site iframe placeholder does not leave dangling pointers when
// `ResolveCrossSiteFrameContent` records cross-site placeholders.
TEST_F(
    AnnotatedPageContentExtractionUtilsTest,
    ResolveCrossSiteFrameContent_UnregisteredOrphanWithNestedCrossSiteIframe) {
  web::FakeWebState web_state;
  web_state.SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  web_state.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  autofill::ChildFrameRegistrar::CreateForWebState(&web_state);
  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::FromWebState(&web_state);
  FrameGrafter grafter;

  autofill::LocalFrameToken orphan_token =
      autofill::LocalFrameToken(base::UnguessableToken::Create());
  FrameGrafter::FrameContent* orphan_content =
      grafter.DeclareContent(orphan_token);
  orphan_content->content.mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  optimization_guide::proto::ContentNode* nested_placeholder =
      orphan_content->content.add_children_nodes();
  nested_placeholder->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  auto* nested_box = nested_placeholder->mutable_content_attributes()
                         ->mutable_geometry()
                         ->mutable_visible_bounding_box();
  nested_box->set_x(10);
  nested_box->set_y(10);
  nested_box->set_width(100);
  nested_box->set_height(50);
  auto* nested_frame_data = nested_placeholder->mutable_content_attributes()
                                ->mutable_iframe_data()
                                ->mutable_frame_data();
  nested_frame_data->set_url("https://cross-site.example/ad");
  nested_frame_data->mutable_security_origin()->set_value(
      "https://cross-site.example");
  grafter.RegisterPlaceholder(
      autofill::RemoteFrameToken(base::UnguessableToken::Create()),
      nested_placeholder);

  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_url("https://example.com");
  apc.mutable_root_node()->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  ResolveCrossSiteFrameContent(
      grafter, registrar, /*include_same_site_only=*/true, &apc,
      web_state.GetWebFramesManager(web::ContentWorld::kPageContentWorld));

  EXPECT_EQ(apc.root_node().children_nodes_size(), 0);
  EXPECT_FALSE(apc.has_gemini_in_chrome_page_metadata());
}

// Tests that PopulateAPCNodeFromContentTree handles CssPosition in geometry.
TEST_F(AnnotatedPageContentExtractionUtilsTest, CssPositionPopulated) {
  optimization_guide::proto::ContentNode node;
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  FrameGrafter grafter;

  base::Value node_content = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 1,
        "geometry": {
          "outerBoundingBox": {
            "x": 0,
            "y": 500,
            "width": 400,
            "height": 100
          },
          "visibleBoundingBox": {
            "x": 0,
            "y": 500,
            "width": 400,
            "height": 100
          },
          "cssPosition": 3
        }
      }
    }
  )");

  ASSERT_TRUE(node_content.is_dict());
  PopulateAPCNodeFromContentTree(
      node_content.GetDict(), origin, grafter,
      /*autofill_context=*/nullptr, &node,
      base::RepeatingCallback<void(bool, const std::string&)>());

  ASSERT_TRUE(node.has_content_attributes());
  ASSERT_TRUE(node.content_attributes().has_geometry());
  EXPECT_EQ(node.content_attributes().geometry().css_position(),
            optimization_guide::proto::CSS_POSITION_FIXED);
}

// Tests that PopulateAPCNodeFromContentTree handles DIALOG_MODELESS
// attributeType.
TEST_F(AnnotatedPageContentExtractionUtilsTest, ModelessDialogPopulated) {
  optimization_guide::proto::ContentNode node;
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  FrameGrafter grafter;

  base::Value node_content = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 29
      }
    }
  )");

  ASSERT_TRUE(node_content.is_dict());
  PopulateAPCNodeFromContentTree(
      node_content.GetDict(), origin, grafter,
      /*autofill_context=*/nullptr, &node,
      base::RepeatingCallback<void(bool, const std::string&)>());

  ASSERT_TRUE(node.has_content_attributes());
  EXPECT_EQ(node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_DIALOG_MODELESS);
}

// Tests that PopulateAPCNodeFromContentTree handles DIALOG_MODAL attributeType.
TEST_F(AnnotatedPageContentExtractionUtilsTest, ModalDialogPopulated) {
  optimization_guide::proto::ContentNode node;
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  FrameGrafter grafter;

  base::Value node_content = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 28
      }
    }
  )");

  ASSERT_TRUE(node_content.is_dict());
  PopulateAPCNodeFromContentTree(
      node_content.GetDict(), origin, grafter,
      /*autofill_context=*/nullptr, &node,
      base::RepeatingCallback<void(bool, const std::string&)>());

  ASSERT_TRUE(node.has_content_attributes());
  EXPECT_EQ(node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_DIALOG_MODAL);
}

// Test that ResolveCrossSiteFrameContent populates
// gemini_in_chrome_page_metadata.screenshot_info (screenshot_size and
// iframe_info with root-relative bounding boxes, URLs, and security origins)
// before redacting cross-site iframe nodes in the APC tree.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       ResolveCrossSiteFrameContentPopulatesScreenshotIframeInfo) {
  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_url("https://example.com/main");
  apc.mutable_viewport_geometry()->set_x(0);
  apc.mutable_viewport_geometry()->set_y(0);
  apc.mutable_viewport_geometry()->set_width(400);
  apc.mutable_viewport_geometry()->set_height(800);

  url::Origin main_origin =
      url::Origin::Create(GURL("https://example.com/main"));
  FrameGrafter grafter;

  base::Value root_json = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 1,
        "geometry": {
          "outerBoundingBox": {"x": 0, "y": 0, "width": 400, "height": 800},
          "visibleBoundingBox": {"x": 0, "y": 0, "width": 400, "height": 800}
        }
      },
      "childrenNodes": [
        {
          "contentAttributes": {
            "attributeType": 3,
            "geometry": {
              "outerBoundingBox": {
                "x": 10,
                "y": 50,
                "width": 300,
                "height": 200
              },
              "visibleBoundingBox": {
                "x": 10,
                "y": 50,
                "width": 300,
                "height": 200
              }
            },
            "iframeData": {
              "remoteFrameToken": {"value": "00112233445566778899aabbccddeeff"},
              "content": {
                "localFrameData": {
                  "sourceUrl": "https://cross-site.org/embed"
                }
              }
            }
          }
        }
      ]
    }
  )");
  ASSERT_TRUE(root_json.is_dict());
  PopulateAPCNodeFromContentTree(
      root_json.GetDict(), main_origin, grafter,
      /*autofill_context=*/nullptr, apc.mutable_root_node(),
      base::RepeatingCallback<void(bool, const std::string&)>());

  auto local_token =
      autofill::LocalFrameToken(base::UnguessableToken::Create());
  std::optional<autofill::RemoteFrameToken> remote_token =
      DeserializeFrameIdAsRemoteFrameToken("00112233445566778899aabbccddeeff");
  ASSERT_TRUE(remote_token.has_value());

  GURL committed_url("https://committed-cross-site.org/redirected");
  auto fake_frame =
      web::FakeWebFrame::Create(local_token.ToString(), /*is_main_frame=*/false,
                                url::Origin::Create(committed_url));
  fake_frame->set_url(committed_url);

  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  frames_manager->AddWebFrame(std::move(fake_frame));

  web::FakeWebState web_state;
  web_state.SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                std::move(frames_manager));
  web_state.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  autofill::ChildFrameRegistrar::CreateForWebState(&web_state);
  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::FromWebState(&web_state);
  registrar->RegisterMapping(*remote_token, local_token);

  ResolveCrossSiteFrameContent(
      grafter, registrar,
      /*include_same_site_only=*/true, &apc,
      web_state.GetWebFramesManager(web::ContentWorld::kPageContentWorld));

  // 1. The cross-site iframe node in `root_node` must be redacted with
  // REASON_CROSS_SITE and have its `frame_data` stripped.
  ASSERT_EQ(apc.root_node().children_nodes_size(), 1);
  const auto& iframe_node = apc.root_node().children_nodes(0);
  EXPECT_FALSE(iframe_node.content_attributes().iframe_data().has_frame_data());
  ASSERT_TRUE(iframe_node.content_attributes()
                  .iframe_data()
                  .has_redacted_frame_metadata());
  EXPECT_EQ(iframe_node.content_attributes()
                .iframe_data()
                .redacted_frame_metadata()
                .reason(),
            optimization_guide::proto::
                IframeData_RedactedFrameMetadata_Reason_REASON_CROSS_SITE);

  // 2. `screenshot_info` must be populated with `screenshot_size` and the
  // cross-site iframe's `IframeInfo` (including its committed security_origin,
  // committed URL, and screenshot-relative bounding box).
  ASSERT_TRUE(apc.has_gemini_in_chrome_page_metadata());
  ASSERT_TRUE(apc.gemini_in_chrome_page_metadata().has_screenshot_info());
  const auto& screenshot_info =
      apc.gemini_in_chrome_page_metadata().screenshot_info();
  EXPECT_EQ(screenshot_info.screenshot_size().width(), 400);
  EXPECT_EQ(screenshot_info.screenshot_size().height(), 800);
  ASSERT_EQ(screenshot_info.iframe_info_size(), 1);

  const auto& info = screenshot_info.iframe_info(0);
  EXPECT_EQ(info.url(), "https://committed-cross-site.org/redirected");
  EXPECT_FALSE(info.security_origin().opaque());
  EXPECT_EQ(info.security_origin().value(), "https://committed-cross-site.org");
  EXPECT_EQ(info.bounding_box().x(), 10);
  EXPECT_EQ(info.bounding_box().y(), 50);
  EXPECT_EQ(info.bounding_box().width(), 300);
  EXPECT_EQ(info.bounding_box().height(), 200);
  EXPECT_TRUE(info.bounding_box().is_screenshot_relative());
}

// Test that `TranslateIframeNodeGeometries` translates both same-origin iframe
// content and cross-origin content grafted by `ResolveCrossSiteFrameContent`,
// which itself leaves the grafted geometry in frame-local coordinates.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       TranslateIframeNodeGeometriesTranslatesSameOriginAndGraftedContent) {
  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_url("https://example.com/main");
  url::Origin main_origin =
      url::Origin::Create(GURL("https://example.com/main"));
  FrameGrafter grafter;

  // Build a main frame containing:
  // 1. A same-origin iframe at (20, 40, 300, 200) with an inline child node at
  //    local (5, 10, 100, 30).
  // 2. A cross-origin iframe placeholder at (30, 300, 300, 200) to be grafted
  //    with a child node at local (8, 12, 120, 40).
  base::Value root_json = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 1,
        "geometry": {
          "outerBoundingBox": {"x": 0, "y": 0, "width": 400, "height": 800},
          "visibleBoundingBox": {"x": 0, "y": 0, "width": 400, "height": 800}
        }
      },
      "childrenNodes": [
        {
          "contentAttributes": {
            "attributeType": 3,
            "geometry": {
              "outerBoundingBox": {
                "x": 20, "y": 40, "width": 300, "height": 200
              },
              "visibleBoundingBox": {
                "x": 20, "y": 40, "width": 300, "height": 200
              }
            },
            "iframeData": {
              "content": {
                "localFrameData": {
                  "sourceUrl": "https://example.com/same-origin"
                }
              }
            }
          },
          "childrenNodes": [
            {
              "contentAttributes": {
                "attributeType": 1,
                "geometry": {
                  "outerBoundingBox": {
                    "x": 0, "y": 0, "width": 300, "height": 200
                  },
                  "visibleBoundingBox": {
                    "x": 0, "y": 0, "width": 300, "height": 200
                  }
                }
              },
              "childrenNodes": [
                {
                  "contentAttributes": {
                    "attributeType": 4,
                    "geometry": {
                      "outerBoundingBox": {
                        "x": 5, "y": 10, "width": 100, "height": 30
                      },
                      "visibleBoundingBox": {
                        "x": 5, "y": 10, "width": 100, "height": 30
                      }
                    }
                  }
                }
              ]
            }
          ]
        },
        {
          "contentAttributes": {
            "attributeType": 3,
            "geometry": {
              "outerBoundingBox": {
                "x": 30, "y": 300, "width": 300, "height": 200
              },
              "visibleBoundingBox": {
                "x": 30, "y": 300, "width": 300, "height": 200
              }
            },
            "iframeData": {
              "remoteFrameToken": {
                "value": "00112233445566778899aabbccddeeff"
              },
              "content": {
                "localFrameData": {
                  "sourceUrl": "https://sub.example.com/cross-origin"
                }
              }
            }
          }
        }
      ]
    }
  )");
  ASSERT_TRUE(root_json.is_dict());
  PopulateAPCNodeFromContentTree(
      root_json.GetDict(), main_origin, grafter,
      /*autofill_context=*/nullptr, apc.mutable_root_node(),
      base::RepeatingCallback<void(bool, const std::string&)>());

  // Declare and populate the cross-origin subframe content.
  auto local_token =
      autofill::LocalFrameToken(base::UnguessableToken::Create());
  std::optional<autofill::RemoteFrameToken> remote_token =
      DeserializeFrameIdAsRemoteFrameToken("00112233445566778899aabbccddeeff");
  ASSERT_TRUE(remote_token.has_value());

  FrameGrafter::FrameContent* child_content =
      grafter.DeclareContent(local_token);
  ASSERT_TRUE(child_content);

  base::Value child_json = base::test::ParseJson(R"(
    {
      "contentAttributes": {
        "attributeType": 1,
        "geometry": {
          "outerBoundingBox": {"x": 0, "y": 0, "width": 300, "height": 200},
          "visibleBoundingBox": {"x": 0, "y": 0, "width": 300, "height": 200}
        }
      },
      "childrenNodes": [
        {
          "contentAttributes": {
            "attributeType": 4,
            "geometry": {
              "outerBoundingBox": {
                "x": 8, "y": 12, "width": 120, "height": 40
              },
              "visibleBoundingBox": {
                "x": 8, "y": 12, "width": 120, "height": 40
              }
            }
          }
        }
      ]
    }
  )");
  ASSERT_TRUE(child_json.is_dict());
  PopulateAPCNodeFromContentTree(
      child_json.GetDict(),
      url::Origin::Create(GURL("https://sub.example.com/cross-origin")),
      grafter, /*autofill_context=*/nullptr, &child_content->content,
      base::RepeatingCallback<void(bool, const std::string&)>());

  web::FakeWebState web_state;
  web_state.SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  web_state.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                std::make_unique<web::FakeWebFramesManager>());
  autofill::ChildFrameRegistrar::CreateForWebState(&web_state);
  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::FromWebState(&web_state);
  registrar->RegisterMapping(*remote_token, local_token);

  ResolveCrossSiteFrameContent(
      grafter, registrar, /*include_same_site_only=*/true, &apc,
      web_state.GetWebFramesManager(web::ContentWorld::kPageContentWorld));

  ASSERT_EQ(apc.root_node().children_nodes_size(), 2);
  const optimization_guide::proto::ContentNode& same_origin_iframe =
      apc.root_node().children_nodes(0);
  ASSERT_EQ(same_origin_iframe.children_nodes_size(), 1);
  const optimization_guide::proto::ContentNode& same_origin_root =
      same_origin_iframe.children_nodes(0);
  ASSERT_EQ(same_origin_root.children_nodes_size(), 1);
  const optimization_guide::proto::ContentNode& cross_origin_iframe =
      apc.root_node().children_nodes(1);
  ASSERT_EQ(cross_origin_iframe.children_nodes_size(), 1);
  const optimization_guide::proto::ContentNode& cross_origin_root =
      cross_origin_iframe.children_nodes(0);
  ASSERT_EQ(cross_origin_root.children_nodes_size(), 1);

  // Grafting alone leaves the grafted content in frame-local coordinates.
  EXPECT_EQ(ToGfxRect(cross_origin_root.children_nodes(0)
                          .content_attributes()
                          .geometry()
                          .outer_bounding_box()),
            gfx::Rect(8, 12, 120, 40));

  TranslateIframeNodeGeometries(&apc);

  // Same-origin iframe content is offset by (20, 40).
  EXPECT_EQ(ToGfxRect(same_origin_root.content_attributes()
                          .geometry()
                          .outer_bounding_box()),
            gfx::Rect(20, 40, 300, 200));
  const optimization_guide::proto::Geometry& same_origin_child_geometry =
      same_origin_root.children_nodes(0).content_attributes().geometry();
  EXPECT_EQ(ToGfxRect(same_origin_child_geometry.outer_bounding_box()),
            gfx::Rect(25, 50, 100, 30));
  EXPECT_EQ(ToGfxRect(same_origin_child_geometry.visible_bounding_box()),
            gfx::Rect(25, 50, 100, 30));

  // Grafted cross-origin content is offset by (30, 300).
  EXPECT_EQ(ToGfxRect(cross_origin_root.content_attributes()
                          .geometry()
                          .outer_bounding_box()),
            gfx::Rect(30, 300, 300, 200));
  const optimization_guide::proto::Geometry& cross_origin_child_geometry =
      cross_origin_root.children_nodes(0).content_attributes().geometry();
  EXPECT_EQ(ToGfxRect(cross_origin_child_geometry.outer_bounding_box()),
            gfx::Rect(38, 312, 120, 40));
  EXPECT_EQ(ToGfxRect(cross_origin_child_geometry.visible_bounding_box()),
            gfx::Rect(38, 312, 120, 40));
}

// Test that `TranslateIframeNodeGeometries` positions the content of an iframe
// partially scrolled above the viewport using the iframe's outer box, rather
// than its clipped visible box, and removes the visible box of content that is
// scrolled out of view.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       TranslateIframeNodeGeometriesUsesIframeOuterBoxOrigin) {
  optimization_guide::proto::AnnotatedPageContent apc;
  // An iframe scrolled 100px above the top of the viewport.
  optimization_guide::proto::ContentNode* iframe =
      AddChildWithGeometry(apc.mutable_root_node(),
                           optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME,
                           /*outer=*/gfx::Rect(20, -100, 300, 400),
                           /*visible=*/gfx::Rect(20, 0, 300, 300));
  optimization_guide::proto::ContentNode* scrolled_out = AddChildWithGeometry(
      iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
      /*outer=*/gfx::Rect(10, 20, 80, 30),
      /*visible=*/gfx::Rect(10, 20, 80, 30));
  optimization_guide::proto::ContentNode* on_screen = AddChildWithGeometry(
      iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
      /*outer=*/gfx::Rect(10, 150, 80, 30),
      /*visible=*/gfx::Rect(10, 150, 80, 30));

  TranslateIframeNodeGeometries(&apc);

  // The iframe itself is in the main frame, so its geometry is unchanged.
  EXPECT_EQ(
      ToGfxRect(iframe->content_attributes().geometry().outer_bounding_box()),
      gfx::Rect(20, -100, 300, 400));

  const optimization_guide::proto::Geometry& scrolled_out_geometry =
      scrolled_out->content_attributes().geometry();
  EXPECT_EQ(ToGfxRect(scrolled_out_geometry.outer_bounding_box()),
            gfx::Rect(30, -80, 80, 30));
  EXPECT_FALSE(scrolled_out_geometry.has_visible_bounding_box());

  const optimization_guide::proto::Geometry& on_screen_geometry =
      on_screen->content_attributes().geometry();
  EXPECT_EQ(ToGfxRect(on_screen_geometry.outer_bounding_box()),
            gfx::Rect(30, 50, 80, 30));
  EXPECT_EQ(ToGfxRect(on_screen_geometry.visible_bounding_box()),
            gfx::Rect(30, 50, 80, 30));
}

// Test that `TranslateIframeNodeGeometries` clips the visible boxes and
// fragments of iframe content to the iframe's visible area, dropping those
// entirely outside of it.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       TranslateIframeNodeGeometriesClipsVisibleBoxesToIframe) {
  optimization_guide::proto::AnnotatedPageContent apc;
  // A 300px tall iframe whose bottom 100px are below the viewport.
  optimization_guide::proto::ContentNode* iframe =
      AddChildWithGeometry(apc.mutable_root_node(),
                           optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME,
                           /*outer=*/gfx::Rect(0, 600, 300, 300),
                           /*visible=*/gfx::Rect(0, 600, 300, 200));
  optimization_guide::proto::ContentNode* straddling = AddChildWithGeometry(
      iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
      /*outer=*/gfx::Rect(10, 180, 80, 40),
      /*visible=*/gfx::Rect(10, 180, 80, 40));
  optimization_guide::proto::Geometry* straddling_geometry =
      straddling->mutable_content_attributes()->mutable_geometry();
  SetBoundingRect(gfx::Rect(10, 180, 80, 20),
                  straddling_geometry->add_fragment_visible_bounding_boxes());
  SetBoundingRect(gfx::Rect(10, 200, 80, 20),
                  straddling_geometry->add_fragment_visible_bounding_boxes());
  optimization_guide::proto::ContentNode* below_viewport = AddChildWithGeometry(
      iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
      /*outer=*/gfx::Rect(10, 250, 80, 30),
      /*visible=*/gfx::Rect(10, 250, 80, 30));

  TranslateIframeNodeGeometries(&apc);

  // Content straddling the bottom of the viewport keeps only its visible part,
  // and its fragment below the viewport is dropped.
  EXPECT_EQ(ToGfxRect(straddling_geometry->outer_bounding_box()),
            gfx::Rect(10, 780, 80, 40));
  EXPECT_EQ(ToGfxRect(straddling_geometry->visible_bounding_box()),
            gfx::Rect(10, 780, 80, 20));
  ASSERT_EQ(straddling_geometry->fragment_visible_bounding_boxes_size(), 1);
  EXPECT_EQ(ToGfxRect(straddling_geometry->fragment_visible_bounding_boxes(0)),
            gfx::Rect(10, 780, 80, 20));

  const optimization_guide::proto::Geometry& below_viewport_geometry =
      below_viewport->content_attributes().geometry();
  EXPECT_EQ(ToGfxRect(below_viewport_geometry.outer_bounding_box()),
            gfx::Rect(10, 850, 80, 30));
  EXPECT_FALSE(below_viewport_geometry.has_visible_bounding_box());
}

// Test that `TranslateIframeNodeGeometries` translates the content of an
// iframe that isn't visible at all, and removes that content's visible boxes.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       TranslateIframeNodeGeometriesTranslatesOffscreenIframeContent) {
  optimization_guide::proto::AnnotatedPageContent apc;
  // An iframe below the viewport, which has no visible box.
  optimization_guide::proto::ContentNode* iframe = AddChildWithGeometry(
      apc.mutable_root_node(),
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME,
      /*outer=*/gfx::Rect(0, 2000, 300, 200), /*visible=*/std::nullopt);
  optimization_guide::proto::ContentNode* child = AddChildWithGeometry(
      iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
      /*outer=*/gfx::Rect(10, 20, 80, 30),
      /*visible=*/gfx::Rect(10, 20, 80, 30));

  TranslateIframeNodeGeometries(&apc);

  const optimization_guide::proto::Geometry& geometry =
      child->content_attributes().geometry();
  EXPECT_EQ(ToGfxRect(geometry.outer_bounding_box()),
            gfx::Rect(10, 2020, 80, 30));
  EXPECT_FALSE(geometry.has_visible_bounding_box());
}

// Test that `TranslateIframeNodeGeometries` composes the offsets of nested
// iframes, clips nested content to the visible areas of all ancestor iframes,
// and leaves main frame geometry unchanged.
TEST_F(AnnotatedPageContentExtractionUtilsTest,
       TranslateIframeNodeGeometriesComposesNestedIframes) {
  optimization_guide::proto::AnnotatedPageContent apc;
  optimization_guide::proto::ContentNode* main_paragraph = AddChildWithGeometry(
      apc.mutable_root_node(),
      optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
      /*outer=*/gfx::Rect(5, 10, 200, 30),
      /*visible=*/gfx::Rect(5, 10, 200, 30));
  // The outer iframe's visible area ends at y = 380, cutting off the bottom
  // 10px of the inner iframe.
  optimization_guide::proto::ContentNode* outer_iframe =
      AddChildWithGeometry(apc.mutable_root_node(),
                           optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME,
                           /*outer=*/gfx::Rect(100, 200, 500, 400),
                           /*visible=*/gfx::Rect(100, 200, 500, 180));
  optimization_guide::proto::ContentNode* inner_iframe = AddChildWithGeometry(
      outer_iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME,
      /*outer=*/gfx::Rect(30, 40, 200, 150),
      /*visible=*/gfx::Rect(30, 40, 200, 150));
  optimization_guide::proto::ContentNode* inner_child = AddChildWithGeometry(
      inner_iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
      /*outer=*/gfx::Rect(10, 15, 60, 20),
      /*visible=*/gfx::Rect(10, 15, 60, 20));
  // Extends past the right edge of the inner iframe.
  optimization_guide::proto::ContentNode* overflowing_child =
      AddChildWithGeometry(
          inner_iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
          /*outer=*/gfx::Rect(190, 15, 60, 20),
          /*visible=*/gfx::Rect(190, 15, 60, 20));
  // Straddles the outer iframe's clipped bottom edge.
  optimization_guide::proto::ContentNode* clipped_by_outer_child =
      AddChildWithGeometry(
          inner_iframe, optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH,
          /*outer=*/gfx::Rect(10, 130, 60, 20),
          /*visible=*/gfx::Rect(10, 130, 60, 20));

  TranslateIframeNodeGeometries(&apc);

  EXPECT_EQ(
      ToGfxRect(
          main_paragraph->content_attributes().geometry().outer_bounding_box()),
      gfx::Rect(5, 10, 200, 30));
  EXPECT_EQ(
      ToGfxRect(
          inner_iframe->content_attributes().geometry().outer_bounding_box()),
      gfx::Rect(130, 240, 200, 150));
  // The inner iframe's visible box is clipped by the outer iframe.
  EXPECT_EQ(
      ToGfxRect(
          inner_iframe->content_attributes().geometry().visible_bounding_box()),
      gfx::Rect(130, 240, 200, 140));

  const optimization_guide::proto::Geometry& inner_child_geometry =
      inner_child->content_attributes().geometry();
  EXPECT_EQ(ToGfxRect(inner_child_geometry.outer_bounding_box()),
            gfx::Rect(140, 255, 60, 20));
  EXPECT_EQ(ToGfxRect(inner_child_geometry.visible_bounding_box()),
            gfx::Rect(140, 255, 60, 20));

  const optimization_guide::proto::Geometry& overflowing_geometry =
      overflowing_child->content_attributes().geometry();
  EXPECT_EQ(ToGfxRect(overflowing_geometry.outer_bounding_box()),
            gfx::Rect(320, 255, 60, 20));
  EXPECT_EQ(ToGfxRect(overflowing_geometry.visible_bounding_box()),
            gfx::Rect(320, 255, 10, 20));

  // Content of the inner iframe is clipped by the outer iframe's visible area.
  const optimization_guide::proto::Geometry& clipped_by_outer_geometry =
      clipped_by_outer_child->content_attributes().geometry();
  EXPECT_EQ(ToGfxRect(clipped_by_outer_geometry.outer_bounding_box()),
            gfx::Rect(140, 370, 60, 20));
  EXPECT_EQ(ToGfxRect(clipped_by_outer_geometry.visible_bounding_box()),
            gfx::Rect(140, 370, 60, 10));
}
