// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/annotated_page_content_extraction_utils.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/test/values_test_util.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/frame_grafter.h"
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
