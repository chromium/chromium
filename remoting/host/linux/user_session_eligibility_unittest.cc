// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/user_session_eligibility.h"

#include <vector>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

std::vector<base::FilePath> GetTestValidShells() {
  return {base::FilePath("/bin/bash"), base::FilePath("/usr/bin/zsh")};
}

}  // namespace

TEST(UserSessionEligibilityTest, RootIsRejected) {
  EXPECT_FALSE(IsUidAllowedForDesktopSession(0, /*is_greeter=*/false));
}

TEST(UserSessionEligibilityTest, RootGreeterIsRejected) {
  EXPECT_FALSE(IsUidAllowedForDesktopSession(0, /*is_greeter=*/true));
}

TEST(UserSessionEligibilityTest, SystemAccountIsRejected) {
  EXPECT_FALSE(IsUidAllowedForDesktopSession(999, /*is_greeter=*/false));
}

TEST(UserSessionEligibilityTest, SystemAccountGreeterIsAllowed) {
  EXPECT_TRUE(IsUidAllowedForDesktopSession(999, /*is_greeter=*/true));
}

TEST(UserSessionEligibilityTest, NobodyIsRejected) {
  EXPECT_FALSE(IsUidAllowedForDesktopSession(65534, /*is_greeter=*/false));
}

TEST(UserSessionEligibilityTest, NobodyGreeterIsAllowed) {
  EXPECT_TRUE(IsUidAllowedForDesktopSession(65534, /*is_greeter=*/true));
}

TEST(UserSessionEligibilityTest, RegularUserIsAllowed) {
  EXPECT_TRUE(IsUidAllowedForDesktopSession(1000, /*is_greeter=*/false));
}

TEST(UserSessionEligibilityTest, ListedShellIsValid) {
  EXPECT_TRUE(
      IsValidLoginShell(base::FilePath("/bin/bash"), GetTestValidShells()));
}

TEST(UserSessionEligibilityTest, UnlistedShellIsInvalid) {
  EXPECT_FALSE(IsValidLoginShell(base::FilePath("/usr/sbin/nologin"),
                                 GetTestValidShells()));
}

TEST(UserSessionEligibilityTest, EmptyShellIsInvalid) {
  EXPECT_FALSE(IsValidLoginShell(base::FilePath(), GetTestValidShells()));
}

TEST(UserSessionEligibilityTest, ListedNologinShellIsInvalid) {
  std::vector<base::FilePath> valid_shells = GetTestValidShells();
  valid_shells.emplace_back("/usr/sbin/nologin");

  EXPECT_FALSE(
      IsValidLoginShell(base::FilePath("/usr/sbin/nologin"), valid_shells));
}

TEST(UserSessionEligibilityTest, ListedFalseShellIsInvalid) {
  std::vector<base::FilePath> valid_shells = GetTestValidShells();
  valid_shells.emplace_back("/bin/false");

  EXPECT_FALSE(IsValidLoginShell(base::FilePath("/bin/false"), valid_shells));
}

TEST(UserSessionEligibilityTest, ListedNologinShellWithTrailingSlashIsInvalid) {
  std::vector<base::FilePath> valid_shells = GetTestValidShells();
  valid_shells.emplace_back("/usr/sbin/nologin/");

  EXPECT_FALSE(
      IsValidLoginShell(base::FilePath("/usr/sbin/nologin/"), valid_shells));
}

TEST(UserSessionEligibilityTest, GetValidLoginShellsReturnsAbsolutePaths) {
  // The result depends on the host's /etc/shells, so only check that every
  // entry is an absolute path.
  for (const base::FilePath& shell : GetValidLoginShells()) {
    EXPECT_TRUE(shell.IsAbsolute()) << shell;
  }
}

}  // namespace remoting
