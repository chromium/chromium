// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/user_desktop_session_backend.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "remoting/host/linux/login_session_manager.h"
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

}  // namespace

class UserDesktopSessionBackendTest : public testing::Test {
 protected:
  const LoginSessionManager::SessionInfo* SelectBestGraphicalSession(
      base::span<const LoginSessionManager::SessionInfo> sessions) {
    return UserDesktopSessionBackend::SelectBestGraphicalSession(sessions);
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
                    "chrome-remote-desktop-session", /*is_remote=*/false),
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

}  // namespace remoting
