// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signaling_id_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(SignalingIdUtilTest, NormalizeSignalingId) {
  EXPECT_EQ(NormalizeSignalingId("USER@DOMAIN.com"), "user@domain.com");
  EXPECT_EQ(NormalizeSignalingId("user@domain.com"), "user@domain.com");
  EXPECT_EQ(NormalizeSignalingId("USER@DOMAIN.com/RESOURCE"),
            "user@domain.com/RESOURCE");
  EXPECT_EQ(NormalizeSignalingId("USER@DOMAIN.com/"), "user@domain.com/");

  // Jabber ID normalization
  EXPECT_EQ("user.mixed.case@googlemail.com/RESOURCE",
            NormalizeSignalingId("User.Mixed.Case@GOOGLEMAIL.com/RESOURCE"));

  // FTL ID normalization
  EXPECT_EQ("user@domain.com/chromoting_ftl_abc123",
            NormalizeSignalingId("USER@DOMAIN.com/chromoting_ftl_abc123"));
  EXPECT_EQ("user@domain.com/chromoting_ftl_abc123",
            NormalizeSignalingId("  USER@DOMAIN.com/chromoting_ftl_abc123"));
  EXPECT_EQ(
      "usermixedcase@gmail.com/chromoting_ftl_abc123",
      NormalizeSignalingId("User.Mixed.Case@GMAIL.com/chromoting_ftl_abc123"));
  EXPECT_EQ("usermixedcase@gmail.com/chromoting_ftl_abc123",
            NormalizeSignalingId(
                "User.Mixed.Case@GOOGLEMAIL.com/chromoting_ftl_abc123"));
  EXPECT_EQ(
      "user.mixed.case@domain.com/chromoting_ftl_abc123",
      NormalizeSignalingId("User.Mixed.Case@DOMAIN.com/chromoting_ftl_abc123"));
  EXPECT_EQ("invalid.user/chromoting_ftl_abc123",
            NormalizeSignalingId("  Invalid.User/chromoting_ftl_abc123"));
  EXPECT_EQ("invalid.user@/chromoting_ftl_abc123",
            NormalizeSignalingId("  Invalid.User@/chromoting_ftl_abc123"));
  EXPECT_EQ("@gmail.com/chromoting_ftl_abc123",
            NormalizeSignalingId("@googlemail.com/chromoting_ftl_abc123"));
}

TEST(SignalingIdUtilTest, SplitSignalingIdResource) {
  std::string email;
  std::string resource_suffix;

  EXPECT_TRUE(
      SplitSignalingIdResource("user@domain/resource", nullptr, nullptr));
  EXPECT_TRUE(SplitSignalingIdResource("user@domain/resource", &email,
                                       &resource_suffix));
  EXPECT_EQ(email, "user@domain");
  EXPECT_EQ(resource_suffix, "resource");

  EXPECT_FALSE(SplitSignalingIdResource("user@domain", nullptr, nullptr));
  EXPECT_FALSE(
      SplitSignalingIdResource("user@domain", &email, &resource_suffix));
  EXPECT_EQ(email, "user@domain");
  EXPECT_EQ(resource_suffix, "");
}

}  // namespace remoting
