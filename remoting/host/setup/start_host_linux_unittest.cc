// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/start_host_linux.h"

#include <unistd.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "remoting/base/username.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/linux/host_types_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

using testing::AllOf;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Not;

constexpr char kUserName[] = "testuser";

class GetLocalUserNameForStartHostTest : public testing::Test {
 protected:
  base::CommandLine command_line_{base::FilePath("start-host")};
};

TEST_F(GetLocalUserNameForStartHostTest, NoSwitches_ReturnsCurrentOrEmpty) {
  std::string expected = getuid() != 0 ? GetUsername() : "";
  EXPECT_EQ(GetLocalUserNameForStartHost(command_line_), expected);
}

TEST_F(GetLocalUserNameForStartHostTest, UserName_TakesPrecedence) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser");
  command_line_.AppendSwitchASCII("cloud-user", "clouduser@example.com");
  command_line_.AppendSwitchASCII("user-name", "localuser");

  EXPECT_EQ(GetLocalUserNameForStartHost(command_line_), "localuser");
}

TEST_F(GetLocalUserNameForStartHostTest, EmptyUserName_ReturnsEmpty) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser");
  command_line_.AppendSwitchASCII("user-name", "");

  EXPECT_EQ(GetLocalUserNameForStartHost(command_line_), "");
}

TEST_F(GetLocalUserNameForStartHostTest, CorpUser_ReturnsIt) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser");

  EXPECT_EQ(GetLocalUserNameForStartHost(command_line_), "corpuser");
}

TEST_F(GetLocalUserNameForStartHostTest, CorpUserEmail_StripsDomain) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser@example.com");

  EXPECT_EQ(GetLocalUserNameForStartHost(command_line_), "corpuser");
}

TEST_F(GetLocalUserNameForStartHostTest, CorpUser_TakesPrecedenceOverCloud) {
  command_line_.AppendSwitchASCII("cloud-user", "clouduser@example.com");
  command_line_.AppendSwitchASCII("corp-user", "corpuser");

  EXPECT_EQ(GetLocalUserNameForStartHost(command_line_), "corpuser");
}

TEST_F(GetLocalUserNameForStartHostTest, CloudUser_StripsDomain) {
  command_line_.AppendSwitchASCII("cloud-user", "clouduser@example.com");

  EXPECT_EQ(GetLocalUserNameForStartHost(command_line_), "clouduser");
}

TEST_F(GetLocalUserNameForStartHostTest, CloudUserNotEmail_ReturnsEmpty) {
  command_line_.AppendSwitchASCII("cloud-user", "clouduser");

  EXPECT_EQ(GetLocalUserNameForStartHost(command_line_), "");
}

class ResolveTargetHostTypeTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    env_.emplace(temp_dir_.GetPath().Append("session_routing.json"));
  }

  void WriteFileWithMode(const base::FilePath& path, std::string_view content) {
    ASSERT_TRUE(base::WriteFile(path, content));
    ASSERT_TRUE(base::SetPosixFilePermissions(path, 0644));
  }

  void WriteRecord(std::string_view content) {
    WriteFileWithMode(env_->GetHostTypeRecordPath(), content);
  }

  // Returns the resolved host type, and stores what was printed to stderr in
  // `stderr_output_`.
  const HostType* Resolve(std::string_view user_name) {
    testing::internal::CaptureStderr();
    const HostType* host_type =
        ResolveTargetHostType(command_line_, user_name, &*env_);
    stderr_output_ = testing::internal::GetCapturedStderr();
    return host_type;
  }

  const HostType* single_process() { return HostType::Find("single-process"); }
  const HostType* gdm_managed() { return HostType::Find("gdm-managed"); }

  base::ScopedTempDir temp_dir_;
  std::optional<FakeHostTypeEnvironment> env_;
  base::CommandLine command_line_{base::FilePath("start-host")};
  std::string stderr_output_;
};

TEST_F(ResolveTargetHostTypeTest, NothingConfigured_UsesDefault) {
  EXPECT_EQ(Resolve(kUserName), &HostType::GetDefaultHostType());
  EXPECT_THAT(stderr_output_, IsEmpty());
}

TEST_F(ResolveTargetHostTypeTest, NothingConfigured_UsesRequestedHostType) {
  command_line_.AppendSwitchASCII("host-type", "gdm-managed");

  EXPECT_EQ(Resolve(kUserName), gdm_managed());
  EXPECT_THAT(stderr_output_, IsEmpty());
}

TEST_F(ResolveTargetHostTypeTest, UnknownHostType_Fails) {
  command_line_.AppendSwitchASCII("host-type", "bogus");

  EXPECT_EQ(Resolve(kUserName), nullptr);
  EXPECT_THAT(stderr_output_, AllOf(HasSubstr("Invalid host type: bogus"),
                                    HasSubstr("Supported host types:")));
}

TEST_F(ResolveTargetHostTypeTest, UsesConfiguredHostType) {
  WriteRecord(R"({"hostType": "gdm-managed"})");

  EXPECT_EQ(Resolve(kUserName), gdm_managed());
  EXPECT_THAT(stderr_output_, IsEmpty());
}

TEST_F(ResolveTargetHostTypeTest, RequestedHostTypeSameAsConfigured_UsesIt) {
  WriteRecord(R"({"hostType": "gdm-managed"})");
  command_line_.AppendSwitchASCII("host-type", "gdm-managed");

  EXPECT_EQ(Resolve(kUserName), gdm_managed());
  EXPECT_THAT(stderr_output_, IsEmpty());
}

// The record is the only thing that is configured, i.e. there is no host
// config.
TEST_F(ResolveTargetHostTypeTest,
       RequestedHostTypeDiffersFromConfigured_Fails) {
  WriteRecord(R"({"hostType": "gdm-managed"})");
  command_line_.AppendSwitchASCII("host-type", "single-process");

  EXPECT_EQ(Resolve(kUserName), nullptr);
  EXPECT_THAT(
      stderr_output_,
      AllOf(HasSubstr("from 'gdm-managed' to 'single-process'"),
            HasSubstr("  /opt/google/chrome-remote-desktop/migrate-host "
                      "single-process --user-name=testuser\n")));
}

TEST_F(ResolveTargetHostTypeTest,
       RequestedHostTypeDiffersFromConfigured_NoUser_Fails) {
  WriteRecord(R"({"hostType": "single-process"})");
  command_line_.AppendSwitchASCII("host-type", "gdm-managed");

  EXPECT_EQ(Resolve(""), nullptr);
  EXPECT_THAT(
      stderr_output_,
      AllOf(HasSubstr("from 'single-process' to 'gdm-managed'"),
            HasSubstr("  /opt/google/chrome-remote-desktop/migrate-host "
                      "gdm-managed\n"),
            Not(HasSubstr("--user-name"))));
}

TEST_F(ResolveTargetHostTypeTest, InvalidRecord_Fails) {
  WriteRecord("not json");
  command_line_.AppendSwitchASCII("host-type", "gdm-managed");

  EXPECT_EQ(Resolve(kUserName), nullptr);
  EXPECT_THAT(stderr_output_,
              AllOf(HasSubstr(env_->GetHostTypeRecordPath().value()),
                    HasSubstr("is invalid")));
}

// Without a record, any enabled/active single-process unit identifies the
// configured host type as single-process regardless of `local_user_name`.
TEST_F(ResolveTargetHostTypeTest,
       NoRecord_SingleProcessUnitForAnotherUser_DetectsSingleProcess) {
  // A multi-process host config that is left over, with its unit disabled,
  // and another user's single-process unit enabled or active.
  WriteFileWithMode(env_->GetMultiProcessConfigPath(), "{}");
  env_->SetUnitEnabled("chrome-remote-desktop@bob.service");
  command_line_.AppendSwitchASCII("host-type", "gdm-managed");

  for (std::string_view user_name : {"alice", ""}) {
    SCOPED_TRACE(user_name);
    EXPECT_EQ(Resolve(user_name), nullptr);
    EXPECT_THAT(stderr_output_,
                AllOf(HasSubstr("from 'single-process' to 'gdm-managed'"),
                      HasSubstr("  /opt/google/chrome-remote-desktop/"
                                "migrate-host gdm-managed")));
  }
}

class ValidateUserSwitchesTest : public testing::Test {
 protected:
  base::CommandLine command_line_{base::FilePath("start-host")};
};

TEST_F(ValidateUserSwitchesTest, NeitherSwitch_ReturnsTrue) {
  EXPECT_TRUE(ValidateUserSwitches(command_line_));
}

TEST_F(ValidateUserSwitchesTest, CorpUser_ReturnsTrue) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser");
  EXPECT_TRUE(ValidateUserSwitches(command_line_));
}

TEST_F(ValidateUserSwitchesTest, ValidCloudUser_ReturnsTrue) {
  command_line_.AppendSwitchASCII("cloud-user", "user@example.com");
  EXPECT_TRUE(ValidateUserSwitches(command_line_));
}

TEST_F(ValidateUserSwitchesTest, BothSwitches_ReturnsFalse) {
  command_line_.AppendSwitchASCII("corp-user", "corpuser");
  command_line_.AppendSwitchASCII("cloud-user", "user@example.com");
  testing::internal::CaptureStderr();
  EXPECT_FALSE(ValidateUserSwitches(command_line_));
  std::string stderr_output = testing::internal::GetCapturedStderr();
  EXPECT_THAT(stderr_output, HasSubstr("cannot be used together"));
}

TEST_F(ValidateUserSwitchesTest, CloudUserNotEmail_ReturnsFalse) {
  command_line_.AppendSwitchASCII("cloud-user", "notanemail");
  testing::internal::CaptureStderr();
  EXPECT_FALSE(ValidateUserSwitches(command_line_));
  std::string stderr_output = testing::internal::GetCapturedStderr();
  EXPECT_THAT(stderr_output, HasSubstr("requires an email address"));
}

class ConfigureSingleProcessHostSetupAsUserTest : public testing::Test {
 protected:
  const HostType* single_process() { return HostType::Find("single-process"); }
  base::CommandLine command_line_{base::FilePath("start-host")};
};

TEST_F(ConfigureSingleProcessHostSetupAsUserTest, NoUserName_Succeeds) {
  ASSERT_NE(getuid(), 0u);
  ASSERT_TRUE(single_process());
  EXPECT_TRUE(
      ConfigureSingleProcessHostSetupAsUser(command_line_, *single_process()));
}

TEST_F(ConfigureSingleProcessHostSetupAsUserTest,
       MatchingUserName_SucceedsAndRemovesSwitch) {
  ASSERT_NE(getuid(), 0u);
  ASSERT_TRUE(single_process());
  command_line_.AppendSwitchASCII("user-name", GetUsername());
  EXPECT_TRUE(
      ConfigureSingleProcessHostSetupAsUser(command_line_, *single_process()));
  EXPECT_FALSE(command_line_.HasSwitch("user-name"));
}

TEST_F(ConfigureSingleProcessHostSetupAsUserTest, MismatchedUserName_Fails) {
  ASSERT_NE(getuid(), 0u);
  ASSERT_TRUE(single_process());
  command_line_.AppendSwitchASCII("user-name",
                                  "mismatched_nonexistent_user_12345");
  testing::internal::CaptureStderr();
  EXPECT_FALSE(
      ConfigureSingleProcessHostSetupAsUser(command_line_, *single_process()));
  std::string stderr_output = testing::internal::GetCapturedStderr();
  EXPECT_THAT(stderr_output, AllOf(HasSubstr("does not match the current user"),
                                   HasSubstr("Run with sudo")));
}

}  // namespace
}  // namespace remoting
