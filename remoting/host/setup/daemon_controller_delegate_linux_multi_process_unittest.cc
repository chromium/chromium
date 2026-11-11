// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_linux_multi_process.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/linux/host_types_test_util.h"
#include "remoting/host/setup/daemon_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

DaemonController::State GetStateForActiveState(std::string_view active_state) {
  const HostType* host_type = HostType::Find("gdm-managed");
  CHECK(host_type);
  // GetState() doesn't use any files.
  auto env = std::make_unique<FakeHostTypeEnvironment>(
      base::FilePath("/nonexistent/session_routing.json"));
  env->SetUnitActiveState(kMultiProcessHostUnit, active_state);
  DaemonControllerDelegateLinuxMultiProcess delegate(*host_type, "user",
                                                     std::move(env));
  return delegate.GetState();
}

TEST(DaemonControllerDelegateLinuxMultiProcessTest, GetState_Started) {
  EXPECT_EQ(GetStateForActiveState("active"), DaemonController::STATE_STARTED);
  EXPECT_EQ(GetStateForActiveState("reloading"),
            DaemonController::STATE_STARTED);
  EXPECT_EQ(GetStateForActiveState("refreshing"),
            DaemonController::STATE_STARTED);
}

TEST(DaemonControllerDelegateLinuxMultiProcessTest, GetState_Starting) {
  EXPECT_EQ(GetStateForActiveState("activating"),
            DaemonController::STATE_STARTING);
}

TEST(DaemonControllerDelegateLinuxMultiProcessTest, GetState_Stopping) {
  EXPECT_EQ(GetStateForActiveState("deactivating"),
            DaemonController::STATE_STOPPING);
}

TEST(DaemonControllerDelegateLinuxMultiProcessTest, GetState_Stopped) {
  EXPECT_EQ(GetStateForActiveState("inactive"),
            DaemonController::STATE_STOPPED);
  EXPECT_EQ(GetStateForActiveState("failed"), DaemonController::STATE_STOPPED);
}

TEST(DaemonControllerDelegateLinuxMultiProcessTest, GetState_Unknown) {
  // An empty state means that systemctl couldn't be run.
  EXPECT_EQ(GetStateForActiveState(""), DaemonController::STATE_UNKNOWN);
  EXPECT_EQ(GetStateForActiveState("maintenance"),
            DaemonController::STATE_UNKNOWN);
}

}  // namespace
}  // namespace remoting
