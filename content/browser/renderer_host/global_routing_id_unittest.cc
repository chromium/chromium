// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/global_routing_id.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

TEST(GlobalRoutingIDTest, RFHTokenPickleConversions) {
  GlobalRenderFrameHostToken token_1;
  token_1.child_id = 0xfedcba09;
  token_1.frame_token = blink::LocalFrameToken();

  auto pickle_1 = token_1.ToPickle();
  auto token_2 = GlobalRenderFrameHostToken::FromPickle(pickle_1);

  EXPECT_TRUE(token_2);
  EXPECT_EQ(token_1, *token_2);

  auto pickle_2 = token_2->ToPickle();
  auto token_3 = GlobalRenderFrameHostToken::FromPickle(pickle_2);

  EXPECT_TRUE(token_3);
  EXPECT_EQ(token_2, *token_3);
}

TEST(GlobalRoutingIDTest, InvalidPickleConversion_MissingChildID) {
  base::Pickle pickle;
  pickle.WriteUInt64(1234567890);
  pickle.WriteUInt64(9876543210);

  EXPECT_FALSE(GlobalRenderFrameHostToken::FromPickle(pickle));
}

TEST(GlobalRoutingIDTest, InvalidPickleConversion_MissingFrameToken) {
  base::Pickle pickle;
  pickle.WriteInt(0xfedcba09);

  EXPECT_FALSE(GlobalRenderFrameHostToken::FromPickle(pickle));
}

TEST(GlobalRoutingIDTest, InvalidPickleConversion_HalfFrameToken) {
  base::Pickle pickle;
  pickle.WriteInt(0xfedcba09);
  pickle.WriteUInt64(1234567890);

  EXPECT_FALSE(GlobalRenderFrameHostToken::FromPickle(pickle));
}

TEST(GlobalRoutingIDTest, InvalidPickleConversion_ZeroFrameToken) {
  base::Pickle pickle;
  pickle.WriteInt(0xfedcba09);
  pickle.WriteUInt64(0);
  pickle.WriteUInt64(0);

  EXPECT_FALSE(GlobalRenderFrameHostToken::FromPickle(pickle));
}

}  // namespace content
