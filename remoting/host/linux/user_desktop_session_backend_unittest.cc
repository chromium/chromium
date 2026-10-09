// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/user_desktop_session_backend.h"

#include <sys/types.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/host/linux/login_session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

LoginSessionManager::SessionInfo CreateSession(const std::string& session_id,
                                               const std::string& session_class,
                                               const std::string& session_type,
                                               const std::string& state,
                                               const std::string& service = "",
                                               bool is_remote = false) {
  LoginSessionManager::SessionInfo info;
  info.session_id = session_id;
  info.session_class = session_class;
  info.session_type = session_type;
  info.state = state;
  info.service = service;
  info.is_remote = is_remote;
  info.username = "testuser";
  info.uid = 1000;
  return info;
}

PasswdUserInfo CreateUserInfo(const std::string& username,
                              uid_t uid,
                              const std::string& shell) {
  PasswdUserInfo info;
  info.username = username;
  info.uid = uid;
  info.gid = uid;
  info.home_dir = base::FilePath("/home").Append(username);
  info.shell = base::FilePath(shell);
  return info;
}

std::vector<base::FilePath> GetTestValidShells() {
  return {base::FilePath("/bin/bash"), base::FilePath("/usr/bin/zsh"),
          base::FilePath("/usr/sbin/nologin")};
}

}  // namespace

class UserDesktopSessionBackendTest : public testing::Test {
 protected:
  const LoginSessionManager::SessionInfo* SelectBestGraphicalSession(
      base::span<const LoginSessionManager::SessionInfo> sessions) {
    return UserDesktopSessionBackend::SelectBestGraphicalSession(sessions);
  }

  base::expected<void, std::string> CheckCreateRemoteSessionUserInfo(
      std::string_view requested_username,
      const PasswdUserInfo& user_info) {
    return UserDesktopSessionBackend::CheckCreateRemoteSessionUserInfo(
               requested_username, user_info, GetTestValidShells())
        .transform_error(
            [](const Loggable& error) { return error.ToString(); });
  }
};

TEST_F(UserDesktopSessionBackendTest, EmptySessionsReturnsNullptr) {
  std::vector<LoginSessionManager::SessionInfo> sessions;
  EXPECT_EQ(SelectBestGraphicalSession(sessions), nullptr);
}

TEST_F(UserDesktopSessionBackendTest, FiltersNonUserSessions) {
  std::vector<LoginSessionManager::SessionInfo> sessions = {
      CreateSession("1", "greeter", "wayland", "active"),
      CreateSession("2", "lock-screen", "wayland", "active"),
      CreateSession("3", "manager", "wayland", "active"),
  };
  EXPECT_EQ(SelectBestGraphicalSession(sessions), nullptr);
}

TEST_F(UserDesktopSessionBackendTest, FiltersNonGraphicalSessions) {
  std::vector<LoginSessionManager::SessionInfo> sessions = {
      CreateSession("1", "user", "tty", "active"),
      CreateSession("2", "user", "unspecified", "active"),
  };
  EXPECT_EQ(SelectBestGraphicalSession(sessions), nullptr);
}

TEST_F(UserDesktopSessionBackendTest, FiltersClosingSessions) {
  std::vector<LoginSessionManager::SessionInfo> sessions = {
      CreateSession("1", "user", "wayland", "closing"),
      CreateSession("2", "user", "x11", "closing"),
  };
  EXPECT_EQ(SelectBestGraphicalSession(sessions), nullptr);
}

TEST_F(UserDesktopSessionBackendTest, AcceptsWaylandAndX11) {
  std::vector<LoginSessionManager::SessionInfo> wayland_sessions = {
      CreateSession("w1", "user", "wayland", "active"),
  };
  const auto* wayland_best = SelectBestGraphicalSession(wayland_sessions);
  ASSERT_NE(wayland_best, nullptr);
  EXPECT_EQ(wayland_best->session_id, "w1");

  std::vector<LoginSessionManager::SessionInfo> x11_sessions = {
      CreateSession("x1", "user", "x11", "active"),
  };
  const auto* x11_best = SelectBestGraphicalSession(x11_sessions);
  ASSERT_NE(x11_best, nullptr);
  EXPECT_EQ(x11_best->session_id, "x1");
}

TEST_F(UserDesktopSessionBackendTest,
       PrioritizesCrdPamServiceOverActiveAndOnline) {
  std::vector<LoginSessionManager::SessionInfo> sessions = {
      CreateSession("normal_active", "user", "wayland", "active",
                    "gdm-password", /*is_remote=*/false),
      CreateSession("crd_session", "user", "wayland", "online",
                    "chrome-remote-desktop", /*is_remote=*/false),
      CreateSession("normal_online", "user", "wayland", "online",
                    "gdm-password", /*is_remote=*/false),
  };
  const auto* best = SelectBestGraphicalSession(sessions);
  ASSERT_NE(best, nullptr);
  EXPECT_EQ(best->session_id, "crd_session");
}

TEST_F(UserDesktopSessionBackendTest, PrioritizesRemoteOverLocalSession) {
  std::vector<LoginSessionManager::SessionInfo> sessions = {
      CreateSession("local_active", "user", "wayland", "active", "gdm-password",
                    /*is_remote=*/false),
      CreateSession("remote_active", "user", "wayland", "active",
                    "gdm-password", /*is_remote=*/true),
  };
  const auto* best = SelectBestGraphicalSession(sessions);
  ASSERT_NE(best, nullptr);
  EXPECT_EQ(best->session_id, "remote_active");
}

TEST_F(UserDesktopSessionBackendTest, PrioritizesActiveOverOnline) {
  std::vector<LoginSessionManager::SessionInfo> sessions = {
      CreateSession("online_1", "user", "wayland", "online", "gdm-password"),
      CreateSession("active_1", "user", "wayland", "active", "gdm-password"),
  };
  const auto* best = SelectBestGraphicalSession(sessions);
  ASSERT_NE(best, nullptr);
  EXPECT_EQ(best->session_id, "active_1");
}

TEST_F(UserDesktopSessionBackendTest, PicksFirstMatchInSamePriorityTier) {
  std::vector<LoginSessionManager::SessionInfo> sessions = {
      CreateSession("first_active", "user", "wayland", "active"),
      CreateSession("second_active", "user", "x11", "active"),
  };
  const auto* best = SelectBestGraphicalSession(sessions);
  ASSERT_NE(best, nullptr);
  EXPECT_EQ(best->session_id, "first_active");
}

TEST_F(UserDesktopSessionBackendTest, CreateRemoteSessionAllowsRegularUser) {
  EXPECT_THAT(CheckCreateRemoteSessionUserInfo(
                  "alice", CreateUserInfo("alice", 1000, "/bin/bash")),
              base::test::HasValue());
}

TEST_F(UserDesktopSessionBackendTest,
       CreateRemoteSessionRejectsNonCanonicalUsername) {
  EXPECT_THAT(CheckCreateRemoteSessionUserInfo(
                  "Alice", CreateUserInfo("alice", 1000, "/bin/bash")),
              base::test::ErrorIs(testing::HasSubstr("canonical passwd name")));
}

TEST_F(UserDesktopSessionBackendTest, CreateRemoteSessionRejectsRoot) {
  EXPECT_THAT(CheckCreateRemoteSessionUserInfo(
                  "root", CreateUserInfo("root", 0, "/bin/bash")),
              base::test::ErrorIs(testing::HasSubstr("UID 0")));
}

TEST_F(UserDesktopSessionBackendTest, CreateRemoteSessionRejectsSystemUser) {
  EXPECT_THAT(CheckCreateRemoteSessionUserInfo(
                  "daemon", CreateUserInfo("daemon", 999, "/bin/bash")),
              base::test::ErrorIs(testing::HasSubstr("UID 999")));
}

TEST_F(UserDesktopSessionBackendTest, CreateRemoteSessionRejectsNobody) {
  EXPECT_THAT(CheckCreateRemoteSessionUserInfo(
                  "nobody", CreateUserInfo("nobody", 65534, "/bin/bash")),
              base::test::ErrorIs(testing::HasSubstr("UID 65534")));
}

TEST_F(UserDesktopSessionBackendTest, CreateRemoteSessionRejectsUnlistedShell) {
  EXPECT_THAT(CheckCreateRemoteSessionUserInfo(
                  "alice", CreateUserInfo("alice", 1000, "/bin/fish")),
              base::test::ErrorIs(testing::HasSubstr("Login shell")));
}

TEST_F(UserDesktopSessionBackendTest, CreateRemoteSessionRejectsEmptyShell) {
  EXPECT_THAT(CheckCreateRemoteSessionUserInfo(
                  "alice", CreateUserInfo("alice", 1000, "")),
              base::test::ErrorIs(testing::HasSubstr("Login shell")));
}

TEST_F(UserDesktopSessionBackendTest,
       CreateRemoteSessionRejectsListedNologinShell) {
  EXPECT_THAT(CheckCreateRemoteSessionUserInfo(
                  "alice", CreateUserInfo("alice", 1000, "/usr/sbin/nologin")),
              base::test::ErrorIs(testing::HasSubstr("Login shell")));
}

}  // namespace remoting
