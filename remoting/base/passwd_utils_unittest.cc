// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/passwd_utils.h"

#include <unistd.h>

#include "build/build_config.h"
#include "remoting/base/username.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(PasswdUtilsTest, GetPasswdUserInfoCurrentProcessUser) {
  std::string username = GetUsername();
  ASSERT_FALSE(username.empty());

  auto result = GetPasswdUserInfo(username);
  ASSERT_TRUE(result.has_value()) << result.error();

  const PasswdUserInfo& user_info = *result;
  EXPECT_EQ(user_info.username, username);
  EXPECT_EQ(user_info.uid, getuid());
  EXPECT_EQ(user_info.gid, getgid());
  EXPECT_FALSE(user_info.home_dir.empty());
#if BUILDFLAG(IS_LINUX)
  EXPECT_FALSE(user_info.supplementary_gids.empty());
  EXPECT_THAT(user_info.supplementary_gids, testing::Contains(user_info.gid));
#endif
}

TEST(PasswdUtilsTest, GetPasswdUserInfoNonexistentUser) {
  auto result = GetPasswdUserInfo("nonexistent_user_123456789_test");
  EXPECT_FALSE(result.has_value());
}

}  // namespace remoting
