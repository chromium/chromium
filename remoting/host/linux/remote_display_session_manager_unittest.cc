// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/remote_display_session_manager.h"

#include <string>

#include "base/no_destructor.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/login_session_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr char kDisplayName[] = "test_display";

const gvariant::ObjectPath& DisplayPath1() {
  static const base::NoDestructor<gvariant::ObjectPath> path(
      *gvariant::ObjectPath::TryFrom("/org/gnome/DisplayManager/Displays/_1"));
  return *path;
}

const gvariant::ObjectPath& DisplayPath2() {
  static const base::NoDestructor<gvariant::ObjectPath> path(
      *gvariant::ObjectPath::TryFrom("/org/gnome/DisplayManager/Displays/_2"));
  return *path;
}

const gvariant::ObjectPath& RemoteId() {
  static const base::NoDestructor<gvariant::ObjectPath> id(
      *gvariant::ObjectPath::TryFrom(
          "/com/google/ChromeRemoteDesktop/RemoteDisplays/test_display"));
  return *id;
}

LoginSessionManager::SessionInfo CreateFakeSessionInfo(
    const std::string& session_id,
    const gvariant::ObjectPath& object_path) {
  LoginSessionManager::SessionInfo info;
  info.is_remote = true;
  info.session_id = session_id;
  info.session_class = "user";
  info.session_type = "wayland";
  info.username = "nobody";
  info.uid = 65534;
  info.object_path = object_path;
  return info;
}

}  // namespace

class RemoteDisplaySessionManagerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  RemoteDisplaySessionManager manager_;

  void AddRemoteDisplay(std::string_view display_name,
                        RemoteDisplaySessionManager::RemoteDisplayInfo info)
      NO_THREAD_SAFETY_ANALYSIS {
    manager_.remote_displays_[std::string(display_name)] = std::move(info);
  }

  void AddStartupBlockingQuery(const gvariant::ObjectPath& display_path)
      NO_THREAD_SAFETY_ANALYSIS {
    manager_.session_info_queries_blocking_startup_.insert(display_path);
  }

  bool HasStartupBlockingQuery(const gvariant::ObjectPath& display_path) const
      NO_THREAD_SAFETY_ANALYSIS {
    return manager_.session_info_queries_blocking_startup_.contains(
        display_path);
  }

  void SetStarting(RemoteDisplaySessionManager::Callback init_callback)
      NO_THREAD_SAFETY_ANALYSIS {
    manager_.start_state_ = RemoteDisplaySessionManager::StartState::STARTING;
    manager_.init_callback_ = std::move(init_callback);
  }

  void OnSessionInfoReady(
      const std::string& display_name,
      const gvariant::ObjectPath& display_path,
      base::expected<LoginSessionManager::SessionInfo, Loggable> result) {
    manager_.OnSessionInfoReady(display_name, display_path, std::move(result));
  }

  void OnRemoteDisplayRemoved(const gvariant::ObjectPath& display_path,
                              const gvariant::ObjectPath& remote_id) {
    manager_.OnRemoteDisplayRemoved(display_path, remote_id);
  }
};

TEST_F(RemoteDisplaySessionManagerTest,
       OnSessionInfoReady_RemoteDisplayRemoved) {
  AddStartupBlockingQuery(DisplayPath1());

  // remote_displays_ does not contain kDisplayName.
  auto session_info = CreateFakeSessionInfo("c1", DisplayPath1());
  OnSessionInfoReady(kDisplayName, DisplayPath1(), std::move(session_info));

  // Should safely return without crash or inserting a phantom display.
  EXPECT_TRUE(manager_.remote_displays().empty());
  EXPECT_FALSE(HasStartupBlockingQuery(DisplayPath1()));
}

TEST_F(RemoteDisplaySessionManagerTest,
       OnSessionInfoReady_DisplaySessionRemoved) {
  // Setup remote display with only DisplayPath2.
  RemoteDisplaySessionManager::RemoteDisplayInfo display_info;
  display_info.sessions[DisplayPath2()] =
      RemoteDisplaySessionManager::RemoteDisplaySession();
  AddRemoteDisplay(kDisplayName, std::move(display_info));
  AddStartupBlockingQuery(DisplayPath1());

  // OnSessionInfoReady arrives for DisplayPath1 (which was removed).
  auto session_info = CreateFakeSessionInfo("c1", DisplayPath1());
  OnSessionInfoReady(kDisplayName, DisplayPath1(), std::move(session_info));

  // Should not resurrect DisplayPath1 into sessions.
  const auto* info = manager_.GetRemoteDisplayInfo(kDisplayName);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->sessions.size(), 1u);
  EXPECT_TRUE(info->sessions.contains(DisplayPath2()));
  EXPECT_FALSE(info->sessions.contains(DisplayPath1()));
  EXPECT_FALSE(HasStartupBlockingQuery(DisplayPath1()));
}

TEST_F(RemoteDisplaySessionManagerTest,
       OnRemoteDisplayRemoved_UnblocksStartup) {
  base::test::TestFuture<base::expected<void, Loggable>> init_future;
  SetStarting(init_future.GetCallback());
  AddStartupBlockingQuery(DisplayPath1());

  OnRemoteDisplayRemoved(DisplayPath1(), RemoteId());

  EXPECT_FALSE(HasStartupBlockingQuery(DisplayPath1()));
  EXPECT_TRUE(init_future.IsReady());
  EXPECT_TRUE(init_future.Get().has_value());
}

TEST_F(RemoteDisplaySessionManagerTest, OnSessionInfoReady_PopulatesSession) {
  RemoteDisplaySessionManager::RemoteDisplayInfo display_info;
  display_info.sessions[DisplayPath1()] =
      RemoteDisplaySessionManager::RemoteDisplaySession();
  AddRemoteDisplay(kDisplayName, std::move(display_info));
  AddStartupBlockingQuery(DisplayPath1());

  auto session_info = CreateFakeSessionInfo("c1", DisplayPath1());
  OnSessionInfoReady(kDisplayName, DisplayPath1(), std::move(session_info));

  const auto* info = manager_.GetRemoteDisplayInfo(kDisplayName);
  ASSERT_NE(info, nullptr);
  auto it = info->sessions.find(DisplayPath1());
  ASSERT_NE(it, info->sessions.end());
  ASSERT_TRUE(it->second.session_info.has_value());
  EXPECT_EQ(it->second.session_info->session_id, "c1");
  EXPECT_FALSE(HasStartupBlockingQuery(DisplayPath1()));
}

}  // namespace remoting
