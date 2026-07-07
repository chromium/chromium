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
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

using AnnotatedPageContentExtractionUtilsTest = PlatformTest;

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
  AutofillExtractionContext context(nullptr, std::nullopt, false, &section_numbers);
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
  AutofillExtractionContext context(nullptr, std::nullopt, false,
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
  AutofillExtractionContext context(nullptr, std::nullopt, false, &section_numbers);
  PopulateAPCNodeFromContentTree(
      node_content.GetDict(), origin, grafter, &context, &node,
      base::BindRepeating(
          [](bool is_focused, const std::string& document_id) {}));

  ASSERT_TRUE(node.has_content_attributes());
  EXPECT_FALSE(node.content_attributes().has_geometry());
}

// Tests that ResolveCrossSiteFrameContent redacts placeholders that are left
// unresolved and are cross-site. Same-site cross-origin placeholders are not
// redacted.
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
  same_origin_placeholder->mutable_content_attributes()
      ->mutable_iframe_data()
      ->mutable_frame_data()
      ->set_url("https://example.com/same-origin");
  grafter.RegisterPlaceholder(same_origin_token, same_origin_placeholder);

  // Create same-site cross-origin placeholder (not grafted).
  optimization_guide::proto::ContentNode* same_site_placeholder =
      apc.mutable_root_node()->add_children_nodes();
  same_site_placeholder->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  same_site_placeholder->mutable_content_attributes()
      ->mutable_iframe_data()
      ->mutable_frame_data()
      ->set_url("https://sub.example.com/same-site");
  grafter.RegisterPlaceholder(same_site_token, same_site_placeholder);

  // Create cross-site placeholder (not grafted).
  optimization_guide::proto::ContentNode* cross_site_placeholder =
      apc.mutable_root_node()->add_children_nodes();
  cross_site_placeholder->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  cross_site_placeholder->mutable_content_attributes()
      ->mutable_iframe_data()
      ->mutable_frame_data()
      ->set_url("https://different-domain.com/cross-site");
  grafter.RegisterPlaceholder(cross_site_token, cross_site_placeholder);

  ResolveCrossSiteFrameContent(grafter, registrar, &apc);

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
