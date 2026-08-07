// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/frame_grafter.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/unguessable_token.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

autofill::LocalFrameToken CreateLocalToken() {
  return autofill::LocalFrameToken(base::UnguessableToken::Create());
}

autofill::RemoteFrameToken CreateRemoteToken() {
  return autofill::RemoteFrameToken(base::UnguessableToken::Create());
}

}  // namespace

using FrameGrafterTest = PlatformTest;

// Tests that grafting works when placeholder and content are matched via
// mapping (Rich Extraction partial merge).
TEST_F(FrameGrafterTest, GraftContent_RichExtraction_PartialMerge) {
  FrameGrafter grafter;

  autofill::LocalFrameToken local_token = CreateLocalToken();
  autofill::RemoteFrameToken remote_token = CreateRemoteToken();

  // Set a placeholder that already has its attribute type populated to trigger
  // the partial merge. This is for Rich Extraction.
  optimization_guide::proto::ContentNode placeholder;
  placeholder.mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  grafter.RegisterPlaceholder(remote_token, &placeholder);

  FrameGrafter::FrameContent* content = grafter.DeclareContent(local_token);
  content->content.mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content("Grafted Content");
  content->frame_data.set_title("Grafted Title");

  // Nothing grafted yet.
  EXPECT_EQ(placeholder.children_nodes_size(), 0);

  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken remote, autofill::LocalFrameToken local,
         autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> {
        if (requested_remote == remote) {
          return local;
        }
        return std::nullopt;
      },
      remote_token, local_token);

  // Use a placer that fails if called because it should not be needed in this
  // test as all the content is mapped to a placeholder.
  auto placer = base::BindRepeating(
      [](FrameGrafter::FrameContent unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer, base::DoNothing());

  // Verify that the placeholder was filled with the content as a child node.
  ASSERT_EQ(placeholder.children_nodes_size(), 1);
  EXPECT_EQ(placeholder.children_nodes(0)
                .content_attributes()
                .text_data()
                .text_content(),
            "Grafted Content");
  EXPECT_EQ(placeholder.content_attributes().iframe_data().frame_data().title(),
            "Grafted Title");
}

// Tests that grafting works when placeholder and content are matched via
// mapping (Light Extraction full replacement).
TEST_F(FrameGrafterTest, GraftContent_LightExtraction_FullReplacement) {
  FrameGrafter grafter;
  autofill::LocalFrameToken local_token = CreateLocalToken();
  autofill::RemoteFrameToken remote_token = CreateRemoteToken();
  optimization_guide::proto::ContentNode placeholder;
  // No attribute type set, so it triggers full replacement.

  grafter.RegisterPlaceholder(remote_token, &placeholder);

  FrameGrafter::FrameContent* content = grafter.DeclareContent(local_token);
  content->content.mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content("Grafted Content");

  // In Light Extraction, frame_data is not merged separately.
  content->frame_data.set_title("Ignored Title");

  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken remote, autofill::LocalFrameToken local,
         autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> {
        if (requested_remote == remote) {
          return local;
        }
        return std::nullopt;
      },
      remote_token, local_token);

  auto placer = base::BindRepeating(
      [](FrameGrafter::FrameContent unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer, base::DoNothing());

  // Verify that the placeholder was replaced by the content.
  EXPECT_EQ(placeholder.content_attributes().text_data().text_content(),
            "Grafted Content");
  // frame_data was ignored/lost in replacement.
  EXPECT_FALSE(placeholder.content_attributes().has_iframe_data());
}

// Tests that unregistered frame content that can't be mapped is correctly
// placed.
TEST_F(FrameGrafterTest, ResolveUnmappedFrames) {
  FrameGrafter grafter;
  autofill::LocalFrameToken local_token = CreateLocalToken();

  FrameGrafter::FrameContent* content = grafter.DeclareContent(local_token);
  content->content.mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content("Unregistered Content");
  content->frame_data.set_title("Unregistered Title");

  // Use mapping lookup that doesn't map to anything where the `placer` will be
  // used as a fallback.
  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> { return std::nullopt; });

  int call_count = 0;
  auto placer = base::BindRepeating(
      [](int* count, FrameGrafter::FrameContent unregistered) {
        (*count)++;
        EXPECT_EQ(unregistered.content.content_attributes()
                      .text_data()
                      .text_content(),
                  "Unregistered Content");
        EXPECT_EQ(unregistered.frame_data.title(), "Unregistered Title");
      },
      &call_count);

  grafter.ResolveUnregisteredContent(mapping_lookup, placer, base::DoNothing());

  EXPECT_EQ(call_count, 1);
}

// Tests that double placeholder registration is ignored.
TEST_F(FrameGrafterTest, DoubleRegistration) {
  FrameGrafter grafter;
  autofill::RemoteFrameToken remote_token = CreateRemoteToken();
  autofill::LocalFrameToken local_token = CreateLocalToken();
  optimization_guide::proto::ContentNode placeholder1;
  placeholder1.mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  optimization_guide::proto::ContentNode placeholder2;
  placeholder2.mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  grafter.RegisterPlaceholder(remote_token, &placeholder1);
  grafter.RegisterPlaceholder(remote_token, &placeholder2);

  FrameGrafter::FrameContent* content = grafter.DeclareContent(local_token);
  content->content.mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content("Grafted Content");

  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken remote, autofill::LocalFrameToken local,
         autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> {
        if (requested_remote == remote) {
          return local;
        }
        return std::nullopt;
      },
      remote_token, local_token);

  auto placer = base::BindRepeating(
      [](FrameGrafter::FrameContent unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer, base::DoNothing());

  ASSERT_EQ(placeholder1.children_nodes_size(), 1);
  EXPECT_EQ(placeholder1.children_nodes(0)
                .content_attributes()
                .text_data()
                .text_content(),
            "Grafted Content");
  // placeholder2 should be untouched/empty (and specifically have 0 children)
  // because the second registration is ignored.
  EXPECT_EQ(placeholder2.children_nodes_size(), 0);
}

// Tests that double added content is ignored.
TEST_F(FrameGrafterTest, DoubleDeclareContent) {
  FrameGrafter grafter;
  autofill::RemoteFrameToken remote_token = CreateRemoteToken();
  autofill::LocalFrameToken local_token = CreateLocalToken();

  FrameGrafter::FrameContent* content1 = grafter.DeclareContent(local_token);
  content1->content.mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content("Content 1");
  // Declare the same token again should return nullptr.
  FrameGrafter::FrameContent* content2 = grafter.DeclareContent(local_token);
  EXPECT_FALSE(content2);

  optimization_guide::proto::ContentNode placeholder;
  placeholder.mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  grafter.RegisterPlaceholder(remote_token, &placeholder);

  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken remote, autofill::LocalFrameToken local,
         autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> {
        if (requested_remote == remote) {
          return local;
        }
        return std::nullopt;
      },
      remote_token, local_token);

  auto placer = base::BindRepeating(
      [](FrameGrafter::FrameContent unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer, base::DoNothing());

  // Should get the first content.
  ASSERT_EQ(placeholder.children_nodes_size(), 1);
  EXPECT_EQ(placeholder.children_nodes(0)
                .content_attributes()
                .text_data()
                .text_content(),
            "Content 1");
}

// Tests that grafting works when the declared content has
// redacted_frame_metadata.
TEST_F(FrameGrafterTest, GraftContent_RedactedIframe) {
  FrameGrafter grafter;
  autofill::LocalFrameToken local_token = CreateLocalToken();
  autofill::RemoteFrameToken remote_token = CreateRemoteToken();
  optimization_guide::proto::ContentNode placeholder;

  placeholder.mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  placeholder.mutable_content_attributes()
      ->mutable_iframe_data()
      ->mutable_frame_data()
      ->set_title("Original Title");

  grafter.RegisterPlaceholder(remote_token, &placeholder);

  FrameGrafter::FrameContent* content = grafter.DeclareContent(local_token);
  content->content.mutable_content_attributes()
      ->mutable_iframe_data()
      ->mutable_redacted_frame_metadata()
      ->set_reason(
          optimization_guide::proto::
              IframeData_RedactedFrameMetadata_Reason_REASON_CROSS_SITE);

  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken remote, autofill::LocalFrameToken local,
         autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> {
        if (requested_remote == remote) {
          return local;
        }
        return std::nullopt;
      },
      remote_token, local_token);

  auto placer = base::BindRepeating(
      [](FrameGrafter::FrameContent unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer, base::DoNothing());

  // No children nodes should be added.
  EXPECT_EQ(placeholder.children_nodes_size(), 0);

  // The placeholder's iframe_data should now contain the redacted metadata.
  EXPECT_TRUE(placeholder.content_attributes()
                  .iframe_data()
                  .has_redacted_frame_metadata());
  EXPECT_EQ(placeholder.content_attributes()
                .iframe_data()
                .redacted_frame_metadata()
                .reason(),
            optimization_guide::proto::
                IframeData_RedactedFrameMetadata_Reason_REASON_CROSS_SITE);

  // The original frame_data (e.g. "Original Title") should be cleared because
  // it's a oneof.
  EXPECT_FALSE(placeholder.content_attributes().iframe_data().has_frame_data());
}

// Tests that ResolveUnregisteredContent invokes the unresolved callback for
// placeholders that couldn't be resolved.
TEST_F(FrameGrafterTest, UnresolvedPlaceholdersHandled) {
  FrameGrafter grafter;
  autofill::RemoteFrameToken remote_token = CreateRemoteToken();
  optimization_guide::proto::ContentNode placeholder;
  grafter.RegisterPlaceholder(remote_token, &placeholder);

  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> { return std::nullopt; });

  auto placer = base::BindRepeating(
      [](FrameGrafter::FrameContent unregistered) { FAIL(); });

  std::vector<optimization_guide::proto::ContentNode*> unresolved;
  auto unresolved_handler = base::BindRepeating(
      [](std::vector<optimization_guide::proto::ContentNode*>* unresolved_list,
         optimization_guide::proto::ContentNode* unresolved_placeholder) {
        unresolved_list->push_back(unresolved_placeholder);
      },
      &unresolved);

  grafter.ResolveUnregisteredContent(mapping_lookup, placer,
                                     unresolved_handler);

  ASSERT_EQ(unresolved.size(), 1u);
  EXPECT_EQ(unresolved[0], &placeholder);
}

// Test that redaction bounding boxes from root and child frames are translated
// by the placeholder origin and collected via post-assembly tree traversal.
TEST_F(FrameGrafterTest, TranslatesFormControlBoundingBoxesForRedaction) {
  FrameGrafter grafter;
  autofill::LocalFrameToken local_token = CreateLocalToken();
  autofill::RemoteFrameToken remote_token = CreateRemoteToken();

  optimization_guide::proto::ContentNode root_node;

  // Root frame child node with a redaction box at (10, 20, 100, 30).
  auto* root_control = root_node.add_children_nodes();
  root_control->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_redaction_decision(
          optimization_guide::proto::
              REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD);
  auto* root_box = root_control->mutable_content_attributes()
                       ->mutable_geometry()
                       ->mutable_visible_bounding_box();
  root_box->set_x(10);
  root_box->set_y(20);
  root_box->set_width(100);
  root_box->set_height(30);

  // Placeholder positioned at (50, 100, 400, 300).
  auto* placeholder = root_node.add_children_nodes();
  placeholder->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  auto* iframe_box = placeholder->mutable_content_attributes()
                         ->mutable_geometry()
                         ->mutable_visible_bounding_box();
  iframe_box->set_x(50);
  iframe_box->set_y(100);
  iframe_box->set_width(400);
  iframe_box->set_height(300);
  grafter.RegisterPlaceholder(remote_token, placeholder);

  // Subframe has a child node with redaction decision at local coordinates (15,
  // 25, 80, 20).
  FrameGrafter::FrameContent* content = grafter.DeclareContent(local_token);
  auto* sub_control = content->content.add_children_nodes();
  sub_control->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_redaction_decision(
          optimization_guide::proto::
              REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);
  auto* sub_box = sub_control->mutable_content_attributes()
                      ->mutable_geometry()
                      ->mutable_visible_bounding_box();
  sub_box->set_x(15);
  sub_box->set_y(25);
  sub_box->set_width(80);
  sub_box->set_height(20);

  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken remote, autofill::LocalFrameToken local,
         autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> {
        if (requested_remote == remote) {
          return local;
        }
        return std::nullopt;
      },
      remote_token, local_token);

  auto placer = base::BindRepeating(
      [](FrameGrafter::FrameContent unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer, base::DoNothing());
  grafter.set_has_sensitive_fields_to_redact(true);
  grafter.CollectFormControlRedactionBoxesFromTree(root_node);

  const auto& boxes = grafter.universal_bounding_boxes_for_redaction();
  ASSERT_EQ(boxes.size(), 2u);

  // Main frame box is untouched: (10, 20, 100, 30).
  EXPECT_TRUE(
      CGRectEqualToRect(boxes[0].visible_box, CGRectMake(10, 20, 100, 30)));
  EXPECT_EQ(
      boxes[0].decision,
      optimization_guide::proto::REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD);

  // Child frame box is shifted by placeholder (50, 100): (65, 125, 80, 20).
  EXPECT_TRUE(
      CGRectEqualToRect(boxes[1].visible_box, CGRectMake(65, 125, 80, 20)));
  EXPECT_EQ(boxes[1].decision,
            optimization_guide::proto::
                REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);
}

// Test that redaction bounding boxes inside multi-level nested iframes (Main ->
// Iframe A -> Iframe B) accumulate ancestor offsets correctly.
TEST_F(FrameGrafterTest, MultiLevelNestedIframeRedactionBoxes) {
  FrameGrafter grafter;
  autofill::LocalFrameToken local_token_a = CreateLocalToken();
  autofill::RemoteFrameToken remote_token_a = CreateRemoteToken();
  autofill::LocalFrameToken local_token_b = CreateLocalToken();
  autofill::RemoteFrameToken remote_token_b = CreateRemoteToken();

  optimization_guide::proto::ContentNode root_node;

  // Iframe A placeholder at (100, 200, 500, 400).
  auto* placeholder_a = root_node.add_children_nodes();
  placeholder_a->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  auto* box_a = placeholder_a->mutable_content_attributes()
                    ->mutable_geometry()
                    ->mutable_visible_bounding_box();
  box_a->set_x(100);
  box_a->set_y(200);
  box_a->set_width(500);
  box_a->set_height(400);
  grafter.RegisterPlaceholder(remote_token_a, placeholder_a);

  // Content A contains Iframe B placeholder at local coordinates (30, 40, 200,
  // 150).
  FrameGrafter::FrameContent* content_a = grafter.DeclareContent(local_token_a);
  auto* placeholder_b = content_a->content.add_children_nodes();
  placeholder_b->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  auto* box_b = placeholder_b->mutable_content_attributes()
                    ->mutable_geometry()
                    ->mutable_visible_bounding_box();
  box_b->set_x(30);
  box_b->set_y(40);
  box_b->set_width(200);
  box_b->set_height(150);
  grafter.RegisterPlaceholder(remote_token_b, placeholder_b);

  // Content B contains a sensitive payment form control at local coordinates
  // (10, 15, 60, 20).
  FrameGrafter::FrameContent* content_b = grafter.DeclareContent(local_token_b);
  auto* sub_control = content_b->content.add_children_nodes();
  sub_control->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_redaction_decision(
          optimization_guide::proto::
              REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);
  auto* sub_box = sub_control->mutable_content_attributes()
                      ->mutable_geometry()
                      ->mutable_visible_bounding_box();
  sub_box->set_x(10);
  sub_box->set_y(15);
  sub_box->set_width(60);
  sub_box->set_height(20);

  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken rem_a, autofill::LocalFrameToken loc_a,
         autofill::RemoteFrameToken rem_b, autofill::LocalFrameToken loc_b,
         autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> {
        if (requested_remote == rem_a) {
          return loc_a;
        }
        if (requested_remote == rem_b) {
          return loc_b;
        }
        return std::nullopt;
      },
      remote_token_a, local_token_a, remote_token_b, local_token_b);

  auto placer = base::BindRepeating(
      [](FrameGrafter::FrameContent unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer, base::DoNothing());
  grafter.set_has_sensitive_fields_to_redact(true);
  grafter.CollectFormControlRedactionBoxesFromTree(root_node);

  const auto& boxes = grafter.universal_bounding_boxes_for_redaction();
  ASSERT_EQ(boxes.size(), 1u);

  // Expected offset = (100 + 30 + 10, 200 + 40 + 15) = (140, 255).
  EXPECT_TRUE(
      CGRectEqualToRect(boxes[0].visible_box, CGRectMake(140, 255, 60, 20)));
  EXPECT_EQ(boxes[0].decision,
            optimization_guide::proto::
                REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);
}

// Test that if an intermediate parent iframe has no visible bounding box (e.g.
// display: none), its child redaction boxes are skipped.
TEST_F(FrameGrafterTest, HiddenParentIframeSkipsChildRedactions) {
  FrameGrafter grafter;
  autofill::LocalFrameToken local_token = CreateLocalToken();
  autofill::RemoteFrameToken remote_token = CreateRemoteToken();

  optimization_guide::proto::ContentNode root_node;

  // Iframe placeholder with NO visible bounding box.
  auto* placeholder = root_node.add_children_nodes();
  placeholder->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  grafter.RegisterPlaceholder(remote_token, placeholder);

  // Subframe has a child node with a redaction decision.
  FrameGrafter::FrameContent* content = grafter.DeclareContent(local_token);
  auto* sub_control = content->content.add_children_nodes();
  sub_control->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_redaction_decision(
          optimization_guide::proto::
              REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);
  auto* sub_box = sub_control->mutable_content_attributes()
                      ->mutable_geometry()
                      ->mutable_visible_bounding_box();
  sub_box->set_x(15);
  sub_box->set_y(25);
  sub_box->set_width(80);
  sub_box->set_height(20);

  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken remote, autofill::LocalFrameToken local,
         autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> {
        if (requested_remote == remote) {
          return local;
        }
        return std::nullopt;
      },
      remote_token, local_token);

  auto placer = base::BindRepeating(
      [](FrameGrafter::FrameContent unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer, base::DoNothing());
  grafter.set_has_sensitive_fields_to_redact(true);
  grafter.CollectFormControlRedactionBoxesFromTree(root_node);

  const auto& boxes = grafter.universal_bounding_boxes_for_redaction();
  EXPECT_TRUE(boxes.empty());
}

// Test that CollectFormControlRedactionBoxesFromTree skips tree traversal when
// has_sensitive_fields_to_redact is false.
TEST_F(FrameGrafterTest,
       CollectFormControlRedactionBoxesFromTree_SkipsWhenNoSensitiveFields) {
  FrameGrafter grafter;
  optimization_guide::proto::ContentNode root_node;

  auto* root_control = root_node.add_children_nodes();
  root_control->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_redaction_decision(
          optimization_guide::proto::
              REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD);
  auto* root_box = root_control->mutable_content_attributes()
                       ->mutable_geometry()
                       ->mutable_visible_bounding_box();
  root_box->set_x(10);
  root_box->set_y(20);
  root_box->set_width(100);
  root_box->set_height(30);

  EXPECT_FALSE(grafter.has_sensitive_fields_to_redact());
  grafter.CollectFormControlRedactionBoxesFromTree(root_node);

  EXPECT_TRUE(grafter.universal_bounding_boxes_for_redaction().empty());
}
