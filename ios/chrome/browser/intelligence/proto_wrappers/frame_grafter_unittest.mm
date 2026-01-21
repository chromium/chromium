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
// mapping.
TEST_F(FrameGrafterTest, GraftContent) {
  FrameGrafter grafter;
  autofill::LocalFrameToken local_token = CreateLocalToken();
  autofill::RemoteFrameToken remote_token = CreateRemoteToken();
  optimization_guide::proto::ContentNode placeholder;

  grafter.RegisterPlaceholder(remote_token, &placeholder);

  optimization_guide::proto::ContentNode* content =
      grafter.DeclareContent(local_token);
  content->mutable_content_attributes()->mutable_text_data()->set_text_content(
      "Grafted Content");

  // Nothing grafted yet.
  EXPECT_FALSE(placeholder.has_content_attributes());

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
      [](optimization_guide::proto::ContentNode unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer);

  // Verify that the placeholder was filled with the content.
  EXPECT_EQ(placeholder.content_attributes().text_data().text_content(),
            "Grafted Content");
}

// Tests that unregistered frame content that can't be mapped is correctly
// placed.
TEST_F(FrameGrafterTest, ResolveUnmappedFrames) {
  FrameGrafter grafter;
  autofill::LocalFrameToken local_token = CreateLocalToken();

  optimization_guide::proto::ContentNode* content =
      grafter.DeclareContent(local_token);
  content->mutable_content_attributes()->mutable_text_data()->set_text_content(
      "Unregistered Content");

  // Use mapping lookup that doesn't map to anything where the `placer` will be
  // used as a fallback.
  auto mapping_lookup = base::BindRepeating(
      [](autofill::RemoteFrameToken requested_remote)
          -> std::optional<autofill::LocalFrameToken> { return std::nullopt; });

  int call_count = 0;
  auto placer = base::BindRepeating(
      [](int* count, optimization_guide::proto::ContentNode unregistered) {
        (*count)++;
        EXPECT_EQ(unregistered.content_attributes().text_data().text_content(),
                  "Unregistered Content");
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
  optimization_guide::proto::ContentNode placeholder2;

  grafter.RegisterPlaceholder(remote_token, &placeholder1);
  grafter.RegisterPlaceholder(remote_token, &placeholder2);

  optimization_guide::proto::ContentNode* content =
      grafter.DeclareContent(local_token);
  content->mutable_content_attributes()->mutable_text_data()->set_text_content(
      "Grafted Content");

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
      [](optimization_guide::proto::ContentNode unregistered) { FAIL(); });

  grafter.ResolveUnregisteredContent(mapping_lookup, placer);

  EXPECT_EQ(placeholder1.content_attributes().text_data().text_content(),
            "Grafted Content");
  // placeholder2 should be untouched/empty because the second registration is
  // ignored.
  EXPECT_FALSE(placeholder2.has_content_attributes());
}
