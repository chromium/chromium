// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_win.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/process_info.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "base/win/win_util.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/host_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class DaemonControllerDelegateWinTest : public testing::Test {
 public:
  DaemonControllerDelegateWinTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    delegate_ =
        std::make_unique<DaemonControllerDelegateWin>(temp_dir_.GetPath());
  }

 protected:
  void WriteUnprivilegedConfig(const base::DictValue& config) {
    ASSERT_TRUE(HostConfigToJsonFile(
        config,
        temp_dir_.GetPath().Append(kDefaultUnprivilegedConfigFileName)));
  }

  void WriteFullConfig(const base::DictValue& config) {
    ASSERT_TRUE(HostConfigToJsonFile(
        config, temp_dir_.GetPath().Append(kDefaultHostConfigFile)));
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<DaemonControllerDelegateWin> delegate_;
};

TEST_F(DaemonControllerDelegateWinTest, GetConfigReturnsNulloptWhenNoConfig) {
  EXPECT_FALSE(delegate_->GetConfig().has_value());
}

TEST_F(DaemonControllerDelegateWinTest, GetConfigReturnsUnprivilegedConfig) {
  base::DictValue config;
  config.Set(kHostIdConfigPath, "test_host_id");
  WriteUnprivilegedConfig(config);

  auto result = delegate_->GetConfig();
  ASSERT_TRUE(result.has_value());
  const std::string* host_id = result->FindString("host_id");
  ASSERT_TRUE(host_id);
  EXPECT_EQ(*host_id, "test_host_id");
}

TEST_F(DaemonControllerDelegateWinTest, CheckPermissionReturnsTrue) {
  bool callback_called = false;
  delegate_->CheckPermission(true, base::BindLambdaForTesting([&](bool result) {
                               EXPECT_TRUE(result);
                               callback_called = true;
                             }));
  EXPECT_TRUE(callback_called);
}

TEST_F(DaemonControllerDelegateWinTest, IsPrivilegedReturnsElevatedState) {
  EXPECT_EQ(delegate_->is_privileged(), base::IsCurrentProcessElevated());
}

TEST_F(DaemonControllerDelegateWinTest, UpdateConfigFailsWhenNoFullConfig) {
  base::DictValue new_config;
  new_config.Set(kHostIdConfigPath, "new_host_id");

  bool callback_called = false;
  delegate_->UpdateConfig(
      std::move(new_config),
      base::BindLambdaForTesting([&](DaemonController::AsyncResult result) {
        EXPECT_EQ(result, DaemonController::RESULT_FAILED);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(DaemonControllerDelegateWinTest, UpdateConfigSucceedsAndUpdatesFiles) {
  base::DictValue full_config;
  full_config.Set(kHostIdConfigPath, "old_host_id");
  full_config.Set(kHostOwnerConfigPath, "owner@google.com");
  full_config.Set(kServiceAccountConfigPath, "service_account@google.com");
  WriteFullConfig(full_config);

  base::DictValue new_config;
  new_config.Set(kHostIdConfigPath, "new_host_id");

  bool callback_called = false;
  delegate_->UpdateConfig(
      std::move(new_config),
      base::BindLambdaForTesting([&](DaemonController::AsyncResult result) {
        EXPECT_EQ(result, DaemonController::RESULT_OK);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);

  // Verify the unprivileged config is updated.
  auto unprivileged_result = delegate_->GetConfig();
  ASSERT_TRUE(unprivileged_result.has_value());
  const std::string* host_id =
      unprivileged_result->FindString(kHostIdConfigPath);
  ASSERT_TRUE(host_id);
  EXPECT_EQ(*host_id, "new_host_id");
}

}  // namespace remoting
