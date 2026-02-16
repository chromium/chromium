// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/frame_grafter.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
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

  grafter.ResolveUnregisteredContent(mapping_lookup, placer);

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

  grafter.ResolveUnregisteredContent(mapping_lookup, placer);

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

  grafter.ResolveUnregisteredContent(mapping_lookup, placer);

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

  grafter.ResolveUnregisteredContent(mapping_lookup, placer);

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

  grafter.ResolveUnregisteredContent(mapping_lookup, placer);

  // Should get the first content.
  ASSERT_EQ(placeholder.children_nodes_size(), 1);
  EXPECT_EQ(placeholder.children_nodes(0)
                .content_attributes()
                .text_data()
                .text_content(),
            "Content 1");
}
