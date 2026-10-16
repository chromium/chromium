// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/agent_integration_configurator_linux.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "remoting/host/agent_integration_configurator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class AgentIntegrationConfiguratorLinuxTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_EXE, temp_dir_.GetPath());
  }

  void WriteBinaryScript(
      std::string_view script_body,
      int permissions = base::FILE_PERMISSION_READ_BY_USER |
                        base::FILE_PERMISSION_WRITE_BY_USER |
                        base::FILE_PERMISSION_EXECUTE_BY_USER) {
    base::FilePath binary_path = temp_dir_.GetPath().AppendASCII(
        AgentIntegrationConfigurator::kBinaryName);
    std::string content = base::StrCat({"#!/bin/sh\n", script_body, "\n"});
    ASSERT_TRUE(base::WriteFile(binary_path, content));
    ASSERT_TRUE(base::SetPosixFilePermissions(binary_path, permissions));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
};

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       IsSupported_FalseWhenBinaryMissing) {
  base::test::TestFuture<bool> future;
  AgentIntegrationConfigurator::IsSupported(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       IsSupported_FalseWhenBinaryNotExecutable) {
  WriteBinaryScript("exit 0", base::FILE_PERMISSION_READ_BY_USER |
                                  base::FILE_PERMISSION_WRITE_BY_USER);
  base::test::TestFuture<bool> future;
  AgentIntegrationConfigurator::IsSupported(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       IsSupported_TrueWhenBinaryExistsAndIsExecutable) {
  WriteBinaryScript("exit 0");
  base::test::TestFuture<bool> future;
  AgentIntegrationConfigurator::IsSupported(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       IsAgentIntegrationEnabled_StatusEnabled) {
  WriteBinaryScript(R"(
if [ "$1" = "status" ]; then
  echo "enabled"
  exit 0
fi
exit 1
)");

  auto configurator = AgentIntegrationConfigurator::Create();
  base::test::TestFuture<bool> future;
  configurator->IsAgentIntegrationEnabled(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       IsAgentIntegrationEnabled_StatusDisabled) {
  WriteBinaryScript(R"(
if [ "$1" = "status" ]; then
  echo "disabled"
  exit 0
fi
exit 1
)");

  auto configurator = AgentIntegrationConfigurator::Create();
  base::test::TestFuture<bool> future;
  configurator->IsAgentIntegrationEnabled(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       IsAgentIntegrationEnabled_UnexpectedOutputOrFailure) {
  WriteBinaryScript(R"(
echo "unexpected"
exit 0
)");

  auto configurator = AgentIntegrationConfigurator::Create();
  base::test::TestFuture<bool> future;
  configurator->IsAgentIntegrationEnabled(future.GetCallback());
  EXPECT_FALSE(future.Get());

  WriteBinaryScript("exit 1");
  base::test::TestFuture<bool> fail_future;
  configurator->IsAgentIntegrationEnabled(fail_future.GetCallback());
  EXPECT_FALSE(fail_future.Get());
}

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       SetAgentIntegrationEnabled_EnableAndDisable) {
  base::FilePath args_file = temp_dir_.GetPath().AppendASCII("last_arg.txt");
  WriteBinaryScript(
      base::StrCat({"echo -n \"$1\" > \"", args_file.value(), "\"\nexit 0"}));

  auto configurator = AgentIntegrationConfigurator::Create();

  base::test::TestFuture<bool> enable_future;
  configurator->SetAgentIntegrationEnabled(true, enable_future.GetCallback());
  EXPECT_TRUE(enable_future.Get());
  std::string recorded_arg;
  ASSERT_TRUE(base::ReadFileToString(args_file, &recorded_arg));
  EXPECT_EQ(recorded_arg, "enable");

  base::test::TestFuture<bool> disable_future;
  configurator->SetAgentIntegrationEnabled(false, disable_future.GetCallback());
  EXPECT_TRUE(disable_future.Get());
  ASSERT_TRUE(base::ReadFileToString(args_file, &recorded_arg));
  EXPECT_EQ(recorded_arg, "disable");
}

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       SetAgentIntegrationEnabled_Failure) {
  WriteBinaryScript("exit 1");

  auto configurator = AgentIntegrationConfigurator::Create();
  base::test::TestFuture<bool> future;
  configurator->SetAgentIntegrationEnabled(true, future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       IsAgentIntegrationEnabled_Timeout) {
  WriteBinaryScript("exec sleep 10");

  AgentIntegrationConfiguratorLinux configurator;
  configurator.SetCommandTimeoutForTesting(base::Milliseconds(50));
  base::test::TestFuture<bool> future;
  configurator.IsAgentIntegrationEnabled(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(AgentIntegrationConfiguratorLinuxTest,
       SetAgentIntegrationEnabled_Timeout) {
  WriteBinaryScript("exec sleep 10");

  AgentIntegrationConfiguratorLinux configurator;
  configurator.SetCommandTimeoutForTesting(base::Milliseconds(50));
  base::test::TestFuture<bool> future;
  configurator.SetAgentIntegrationEnabled(true, future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace remoting
