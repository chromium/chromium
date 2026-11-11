// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using testing::AllOf;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::IsEmpty;

constexpr char kUserName[] = "testuser";
constexpr char kSingleProcessUnit[] = "chrome-remote-desktop@testuser.service";
constexpr char kMultiProcessUnit[] = "chrome-remote-desktop.service";

class HostTypeTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    routing_config_path_ = temp_dir_.GetPath().Append("session_routing.json");
    backup_path_ = temp_dir_.GetPath().Append("session_routing.json.bak");
    env_.emplace(routing_config_path_);
    record_path_ = env_->GetHostTypeRecordPath();
  }

  void WriteRoutingConfig(std::string_view content) {
    WriteFileWithMode(routing_config_path_, content);
  }

  void WriteRecord(std::string_view content) {
    WriteFileWithMode(record_path_, content);
  }

  void WriteFileWithMode(const base::FilePath& path,
                         std::string_view content,
                         int mode = 0644) {
    ASSERT_TRUE(base::WriteFile(path, content));
    ASSERT_TRUE(base::SetPosixFilePermissions(path, mode));
  }

  std::optional<std::string> ReadContent(const base::FilePath& path) {
    std::string content;
    if (!base::ReadFileToString(path, &content)) {
      return std::nullopt;
    }
    return content;
  }

  const HostType* single_process() { return HostType::Find("single-process"); }
  const HostType* gdm_managed() { return HostType::Find("gdm-managed"); }

  base::ScopedTempDir temp_dir_;
  base::FilePath routing_config_path_;
  base::FilePath backup_path_;
  base::FilePath record_path_;
  std::optional<FakeHostTypeEnvironment> env_;
};

TEST_F(HostTypeTest, Properties) {
  EXPECT_FALSE(single_process()->is_multi_process());
  EXPECT_TRUE(single_process()->requires_local_user());
  EXPECT_TRUE(gdm_managed()->is_multi_process());
  EXPECT_FALSE(gdm_managed()->requires_local_user());
}

TEST_F(HostTypeTest, IsUnitActive_TrueForRunningOrStartingUnit) {
  for (std::string_view state :
       {"active", "activating", "reloading", "refreshing"}) {
    SCOPED_TRACE(state);
    env_->SetUnitActiveState(kMultiProcessUnit, state);
    EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
  }
}

TEST_F(HostTypeTest, IsUnitActive_FalseForStoppedOrStoppingUnit) {
  for (std::string_view state : {"inactive", "failed", "deactivating", ""}) {
    SCOPED_TRACE(state);
    env_->SetUnitActiveState(kMultiProcessUnit, state);
    EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
  }
}

TEST_F(HostTypeTest, GetConfigured_ValidRecord) {
  WriteRecord(R"({"hostType": "gdm-managed"})");

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));

  WriteRecord(R"({"hostType": "single-process"})");

  EXPECT_THAT(HostType::GetConfigured(*env_, ""), ValueIs(single_process()));
}

TEST_F(HostTypeTest, GetConfigured_RecordWinsOverHostConfigs) {
  WriteRecord(R"({"hostType": "single-process"})");
  WriteFileWithMode(env_->GetMultiProcessConfigPath(), "{}");

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(single_process()));
}

TEST_F(HostTypeTest, GetConfigured_RecordIgnoresUnitState) {
  // E.g. the host was stopped with `systemctl stop`, or by
  // DaemonController::Stop().
  WriteRecord(R"({"hostType": "gdm-managed"})");
  env_->SetUnitActive(kSingleProcessUnit);

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));
}

TEST_F(HostTypeTest, GetConfigured_NoRecord_UsesFallback) {
  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName), ValueIs(nullptr));

  WriteFileWithMode(env_->GetMultiProcessConfigPath(), "{}");

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));
}

TEST_F(HostTypeTest, GetConfigured_RecordDirectoryAbsent_UsesFallback) {
  FakeHostTypeEnvironment env(
      temp_dir_.GetPath().Append("missing").Append("session_routing.json"));

  EXPECT_THAT(HostType::GetConfigured(env, kUserName), ValueIs(nullptr));
}

TEST_F(HostTypeTest, GetConfigured_MalformedRecord_Fails) {
  for (std::string_view content : {"", "gdm-managed", R"(["gdm-managed"])"}) {
    SCOPED_TRACE(content);
    WriteRecord(content);

    EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
                ErrorIs(AllOf(HasSubstr("It is not a JSON dictionary."),
                              HasSubstr("Fix or delete the file"))));
  }
  for (std::string_view content :
       {R"({"hostType": 1})", R"({"type": "gdm-managed"})"}) {
    SCOPED_TRACE(content);
    WriteRecord(content);

    EXPECT_THAT(
        HostType::GetConfigured(*env_, kUserName),
        ErrorIs(AllOf(HasSubstr(R"(It has no string "hostType" field.)"),
                      HasSubstr("Fix or delete the file"))));
  }
}

TEST_F(HostTypeTest, GetConfigured_UnknownHostType_Fails) {
  WriteRecord(R"({"hostType": "unknown"})");

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ErrorIs(HasSubstr("Unknown host type 'unknown'")));
}

TEST_F(HostTypeTest, GetConfigured_InsecureRecord_Fails) {
  WriteFileWithMode(record_path_, R"({"hostType": "gdm-managed"})", 0666);

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ErrorIs(AllOf(HasSubstr("Failed to read it securely"),
                            HasSubstr("is writable by group or others"))));
}

TEST_F(HostTypeTest, GetConfigured_InsecureRecordDirectory_Fails) {
  base::FilePath directory = temp_dir_.GetPath().Append("insecure");
  ASSERT_TRUE(base::CreateDirectory(directory));
  ASSERT_TRUE(base::SetPosixFilePermissions(directory, 0777));
  FakeHostTypeEnvironment env(directory.Append("session_routing.json"));

  // Even without a record, since an attacker could add one.
  EXPECT_THAT(HostType::GetConfigured(env, kUserName),
              ErrorIs(AllOf(HasSubstr("Failed to read it securely"),
                            HasSubstr("is writable by group or others"))));
}

TEST_F(HostTypeTest, GetConfigured_SymlinkedRecord_Fails) {
  base::FilePath target = temp_dir_.GetPath().Append("target.json");
  WriteFileWithMode(target, R"({"hostType": "gdm-managed"})");
  ASSERT_TRUE(base::CreateSymbolicLink(target, record_path_));

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ErrorIs(AllOf(HasSubstr("Failed to read it securely"),
                            HasSubstr("is a symbolic link"))));
}

TEST_F(HostTypeTest, GetConfigured_OversizedRecord_Fails) {
  WriteRecord(base::StrCat({R"({"hostType": "gdm-managed", "padding": ")",
                            std::string(4096, 'a'), R"("})"}));

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ErrorIs(AllOf(HasSubstr("Failed to read it securely"),
                            HasSubstr("exceeds the size limit"))));
}

TEST_F(HostTypeTest, GetFallbackConfigured_NoHostConfig_ReturnsNull) {
  // Active units and a routing file without a host config are leftovers.
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetUnitActive(kMultiProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, kUserName), nullptr);
  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, ""), nullptr);
}

TEST_F(HostTypeTest, GetFallbackConfigured_MultiProcessConfig_GdmManaged) {
  WriteFileWithMode(env_->GetMultiProcessConfigPath(), "{}");

  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, kUserName), gdm_managed());
  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, ""), gdm_managed());
}

// gdm-managed is the only multi-process host type for now, so it is also used
// if there is a routing file.
TEST_F(HostTypeTest, GetFallbackConfigured_RoutingFile_InterimGdmManaged) {
  WriteFileWithMode(env_->GetMultiProcessConfigPath(), "{}");
  WriteRoutingConfig("{}");

  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, kUserName), gdm_managed());
}

TEST_F(HostTypeTest, GetFallbackConfigured_SingleProcessUnitEnabled) {
  env_->SetUnitEnabled(kSingleProcessUnit);

  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, kUserName),
            single_process());
  // Only the given user's unit is checked.
  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, ""), nullptr);
  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, "otheruser"), nullptr);
}

TEST_F(HostTypeTest,
       GetFallbackConfigured_BothHosts_MultiProcessUnitEnabled_GdmManaged) {
  WriteFileWithMode(env_->GetMultiProcessConfigPath(), "{}");
  env_->SetUnitEnabled(kMultiProcessUnit);
  env_->SetUnitEnabled(kSingleProcessUnit);

  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, kUserName), gdm_managed());
}

// E.g. after `migrate-host single-process --keep-config`.
TEST_F(HostTypeTest,
       GetFallbackConfigured_BothHosts_MultiProcessUnitDisabled_SingleProcess) {
  WriteFileWithMode(env_->GetMultiProcessConfigPath(), "{}");
  env_->SetUnitEnabled(kSingleProcessUnit);

  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, kUserName),
            single_process());
  // Without the user, the single-process host can't be found.
  EXPECT_EQ(HostType::GetFallbackConfigured(*env_, ""), gdm_managed());
}

TEST_F(HostTypeTest, GetFallbackConfigured_DoesNotWriteRecord) {
  WriteFileWithMode(env_->GetMultiProcessConfigPath(), "{}");

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));

  EXPECT_FALSE(base::PathExists(record_path_));
}

TEST_F(HostTypeTest, WriteHostTypeRecord_RoundTrips) {
  EXPECT_TRUE(gdm_managed()->WriteHostTypeRecord(*env_));

  EXPECT_EQ(ReadContent(record_path_), R"({"hostType":"gdm-managed"})");
  int mode = 0;
  ASSERT_TRUE(base::GetPosixFilePermissions(record_path_, &mode));
  EXPECT_EQ(mode, 0644);
  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));

  // Overwrites the existing record.
  EXPECT_TRUE(single_process()->WriteHostTypeRecord(*env_));

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(single_process()));
}

TEST_F(HostTypeTest, WriteHostTypeRecord_CreatesDirectory) {
  base::FilePath directory = temp_dir_.GetPath().Append("missing");
  FakeHostTypeEnvironment env(directory.Append("session_routing.json"));

  EXPECT_TRUE(gdm_managed()->WriteHostTypeRecord(env));

  int mode = 0;
  ASSERT_TRUE(base::GetPosixFilePermissions(directory, &mode));
  EXPECT_EQ(mode, 0755);
  EXPECT_THAT(HostType::GetConfigured(env, kUserName), ValueIs(gdm_managed()));
}

TEST_F(HostTypeTest, WriteHostTypeRecord_FailsIfDirectoryInsecure) {
  base::FilePath directory = temp_dir_.GetPath().Append("insecure");
  ASSERT_TRUE(base::CreateDirectory(directory));
  ASSERT_TRUE(base::SetPosixFilePermissions(directory, 0777));
  FakeHostTypeEnvironment env(directory.Append("session_routing.json"));

  EXPECT_FALSE(gdm_managed()->WriteHostTypeRecord(env));

  EXPECT_FALSE(base::PathExists(env.GetHostTypeRecordPath()));
}

TEST_F(HostTypeTest, WriteHostTypeRecord_ReplacesSymlink) {
  base::FilePath target = temp_dir_.GetPath().Append("target.json");
  WriteFileWithMode(target, "target");
  ASSERT_TRUE(base::CreateSymbolicLink(target, record_path_));

  EXPECT_TRUE(gdm_managed()->WriteHostTypeRecord(*env_));

  EXPECT_FALSE(base::IsLink(record_path_));
  EXPECT_EQ(ReadContent(target), "target");
  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));
}

TEST_F(HostTypeTest, StartHook_WritesRecord) {
  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));

  EXPECT_TRUE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(single_process()));
}

TEST_F(HostTypeTest, StartHook_InvalidContext_DoesNotWriteRecord) {
  WriteRecord(R"({"hostType": "gdm-managed"})");
  std::vector<std::string> log;
  FakeHostType type("fake", log);
  type.set_fail_method("ValidateStartHostHookContext");

  EXPECT_FALSE(type.RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(log, ElementsAre("fake.ValidateStartHostHookContext"));
  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));
}

TEST_F(HostTypeTest, StartHook_RecordWrittenBeforeStateIsChanged) {
  std::vector<std::string> log;
  FakeHostType type("fake", log);
  type.set_fail_method("SetUpState");

  EXPECT_FALSE(type.RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  // The fake host type isn't registered, so the record can't be read back.
  EXPECT_EQ(ReadContent(record_path_), R"({"hostType":"fake"})");
}

TEST_F(HostTypeTest, StartHook_FailedSetUp_RecordNamesTargetHostType) {
  WriteRecord(R"({"hostType": "single-process"})");
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetDisableAndStopUnitFails(kSingleProcessUnit);

  EXPECT_FALSE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  // The record names gdm-managed, so running its start hook again (e.g. with
  // start-host) repairs the setup.
  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));
}

TEST_F(HostTypeTest, StartHook_InsecureRecordDirectory_ChangesNothing) {
  base::FilePath directory = temp_dir_.GetPath().Append("insecure");
  ASSERT_TRUE(base::CreateDirectory(directory));
  ASSERT_TRUE(base::SetPosixFilePermissions(directory, 0777));
  FakeHostTypeEnvironment env(directory.Append("session_routing.json"));
  env.SetUnitActive(kSingleProcessUnit);

  EXPECT_FALSE(gdm_managed()->RunStartHostHook(
      HostTypeContext(env, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env.unit_operations(), IsEmpty());
  EXPECT_FALSE(base::PathExists(env.GetHostTypeRecordPath()));
}

TEST_F(HostTypeTest, StopHook_KeepsRecord) {
  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));
  EXPECT_TRUE(gdm_managed()->RunStopHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(HostType::GetConfigured(*env_, kUserName),
              ValueIs(gdm_managed()));
}

TEST_F(HostTypeTest, SingleProcessStartHook_StartsUnit) {
  WriteRoutingConfig("{}");

  EXPECT_TRUE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"enable ", kSingleProcessUnit})));
  EXPECT_TRUE(env_->IsUnitActive(kSingleProcessUnit));
  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
}

TEST_F(HostTypeTest, SingleProcessStartHook_StopsMultiProcessUnit) {
  // E.g. left enabled after the multi-process host was stopped with
  // `systemctl stop`.
  env_->SetUnitEnabled(kMultiProcessUnit);

  EXPECT_TRUE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kMultiProcessUnit}),
                          base::StrCat({"enable ", kSingleProcessUnit})));
}

TEST_F(HostTypeTest, SingleProcessStartHook_FailsWithoutUser) {
  EXPECT_FALSE(single_process()->ValidateStartHostHookContext(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));
  EXPECT_FALSE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(), IsEmpty());
  EXPECT_FALSE(base::PathExists(record_path_));
}

TEST_F(HostTypeTest, SingleProcessStartHook_FailsWithUnsafeUser) {
  EXPECT_FALSE(single_process()->ValidateStartHostHookContext(
      HostTypeContext(*env_, "user name", /*is_corp_host=*/false)));
  EXPECT_FALSE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, "*", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(), IsEmpty());
  EXPECT_FALSE(base::PathExists(record_path_));
}

TEST_F(HostTypeTest, ValidateStartHostHookContext) {
  EXPECT_TRUE(single_process()->ValidateStartHostHookContext(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));
  EXPECT_TRUE(gdm_managed()->ValidateStartHostHookContext(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));
  EXPECT_TRUE(gdm_managed()->ValidateStartHostHookContext(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/true)));
}

TEST_F(HostTypeTest,
       SingleProcessStartHook_FailsIfMultiProcessUnitFailsToStop) {
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetDisableAndStopUnitFails(kMultiProcessUnit);

  EXPECT_FALSE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_FALSE(env_->IsUnitActive(kSingleProcessUnit));
}

TEST_F(HostTypeTest, SingleProcessStartHook_FailsIfUnitFailsToStart) {
  env_->SetEnableAndStartUnitFails(kSingleProcessUnit);

  EXPECT_FALSE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));
}

TEST_F(HostTypeTest, SingleProcessStopHook_StopsUnit) {
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetUnitEnabled(kSingleProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_TRUE(single_process()->RunStopHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kSingleProcessUnit})));
  EXPECT_FALSE(env_->IsUnitActive(kSingleProcessUnit));
  EXPECT_FALSE(env_->IsUnitEnabled(kSingleProcessUnit));
  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
}

TEST_F(HostTypeTest, SingleProcessStopHook_DisablesEnabledInactiveUnit) {
  env_->SetUnitEnabled(kSingleProcessUnit);

  EXPECT_TRUE(single_process()->RunStopHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_FALSE(env_->IsUnitEnabled(kSingleProcessUnit));
}

TEST_F(HostTypeTest, SingleProcessStopHook_NothingToDoIfUnitInactive) {
  EXPECT_TRUE(single_process()->RunStopHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(), IsEmpty());
}

TEST_F(HostTypeTest, SingleProcessStopHook_NoOpWithoutUser) {
  // The template unit, which would be used if the empty user wasn't skipped.
  env_->SetUnitActive("chrome-remote-desktop@.service");
  env_->SetUnitEnabled("chrome-remote-desktop@.service");

  EXPECT_TRUE(single_process()->RunStopHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(), IsEmpty());
}

TEST_F(HostTypeTest, SingleProcessStopHook_FailsIfUnitFailsToStop) {
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetDisableAndStopUnitFails(kSingleProcessUnit);

  EXPECT_FALSE(single_process()->RunStopHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));
}

TEST_F(HostTypeTest, GdmManagedStartHook_StartsUnitIfRoutingFileAbsent) {
  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"enable ", kMultiProcessUnit})));
  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_FALSE(base::PathExists(backup_path_));
}

TEST_F(HostTypeTest, GdmManagedStartHook_SucceedsIfRoutingDirectoryAbsent) {
  FakeHostTypeEnvironment env(
      temp_dir_.GetPath().Append("missing").Append("session_routing.json"));

  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(env, "", /*is_corp_host=*/false)));

  EXPECT_TRUE(env.IsUnitActive(kMultiProcessUnit));
}

TEST_F(HostTypeTest, GdmManagedStartHook_StopsSingleProcessUnit) {
  // E.g. left enabled after the single-process host was stopped with
  // `systemctl stop`.
  env_->SetUnitEnabled(kSingleProcessUnit);

  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kSingleProcessUnit}),
                          base::StrCat({"enable ", kMultiProcessUnit})));
}

TEST_F(HostTypeTest, GdmManagedStartHook_FailsIfSingleProcessUnitFailsToStop) {
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetDisableAndStopUnitFails(kSingleProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_FALSE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
  EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
}

TEST_F(HostTypeTest, GdmManagedStartHook_StopsSingleProcessUnitsOfAllUsers) {
  env_->SetUnitActive("chrome-remote-desktop@alice.service");
  env_->SetUnitEnabled("chrome-remote-desktop@bob.service");
  // Neither enabled nor active, so it's left alone.
  env_->SetUnitActiveState("chrome-remote-desktop@carol.service", "failed");

  // Also without a local user.
  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre("disable chrome-remote-desktop@alice.service",
                          "disable chrome-remote-desktop@bob.service",
                          base::StrCat({"enable ", kMultiProcessUnit})));
}

TEST_F(HostTypeTest, GdmManagedStartHook_FailsIfUnitsCannotBeListed) {
  env_->SetListUnitsFails();
  WriteRoutingConfig("{}");

  EXPECT_FALSE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(), IsEmpty());
  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
}

TEST_F(HostTypeTest, GdmManagedStartHook_BacksUpRoutingFile) {
  WriteRoutingConfig(R"({"sessionUser": "testuser"})");

  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_EQ(ReadContent(backup_path_), R"({"sessionUser": "testuser"})");
  EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
}

TEST_F(HostTypeTest, GdmManagedStartHook_MovesDanglingSymlink) {
  base::FilePath target = temp_dir_.GetPath().Append("missing.json");
  ASSERT_TRUE(base::CreateSymbolicLink(target, routing_config_path_));

  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_FALSE(base::IsLink(routing_config_path_));
  EXPECT_TRUE(base::IsLink(backup_path_));
  EXPECT_FALSE(base::PathExists(target));
}

TEST_F(HostTypeTest, GdmManagedStartHook_FailsIfBackupFails) {
  // A directory can't replace a file, so the backup fails.
  WriteRoutingConfig("{}");
  ASSERT_TRUE(base::CreateDirectory(backup_path_));
  ASSERT_TRUE(base::WriteFile(backup_path_.Append("file"), "x"));

  EXPECT_FALSE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
  EXPECT_THAT(env_->unit_operations(), IsEmpty());
}

TEST_F(HostTypeTest, GdmManagedStopHook_StopsUnitAndBacksUpRoutingFile) {
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetUnitEnabled(kMultiProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_TRUE(gdm_managed()->RunStopHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kMultiProcessUnit})));
  EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
  EXPECT_FALSE(env_->IsUnitEnabled(kMultiProcessUnit));
  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_EQ(ReadContent(backup_path_), "{}");
}

TEST_F(HostTypeTest, GdmManagedStopHook_KeepsRoutingFileIfUnitFailsToStop) {
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetDisableAndStopUnitFails(kMultiProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_FALSE(gdm_managed()->RunStopHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
  EXPECT_FALSE(base::PathExists(backup_path_));
}

TEST_F(HostTypeTest, SingleProcessStartHook_AlreadySetUp_OnlyStartsHostUnit) {
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetUnitEnabled(kSingleProcessUnit);

  EXPECT_TRUE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"enable ", kSingleProcessUnit})));
  EXPECT_TRUE(env_->IsUnitActive(kSingleProcessUnit));
}

TEST_F(HostTypeTest, Hooks_OrderOfUnitAndStateMethods) {
  std::vector<std::string> log;
  FakeHostType type("fake", log);
  HostTypeContext context(*env_, "", /*is_corp_host=*/false);

  EXPECT_TRUE(type.RunStartHostHook(context));
  EXPECT_TRUE(type.RunStopHostHook(context));

  EXPECT_THAT(log, ElementsAre("fake.ValidateStartHostHookContext",
                               "fake.SetUpState", "fake.StartHostUnit",
                               "fake.StopHostUnit", "fake.CleanUpState"));
}

TEST_F(HostTypeTest, Hooks_StopAtFirstFailure) {
  std::vector<std::string> log;
  FakeHostType type("fake", log);
  HostTypeContext context(*env_, "", /*is_corp_host=*/false);

  type.set_fail_method("SetUpState");
  EXPECT_FALSE(type.RunStartHostHook(context));
  type.set_fail_method("StopHostUnit");
  EXPECT_FALSE(type.RunStopHostHook(context));

  EXPECT_THAT(log, ElementsAre("fake.ValidateStartHostHookContext",
                               "fake.SetUpState", "fake.StopHostUnit"));
}

TEST_F(HostTypeTest, GdmManagedStartHook_AlreadySetUp_OnlyStartsHostUnit) {
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetUnitEnabled(kMultiProcessUnit);

  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"enable ", kMultiProcessUnit})));
  EXPECT_FALSE(base::PathExists(routing_config_path_));
}

TEST_F(HostTypeTest, NeedsStartHostHook_SingleProcess) {
  EXPECT_FALSE(single_process()->NeedsStartHostHook(*env_));

  env_->SetUnitEnabled(kMultiProcessUnit);
  EXPECT_TRUE(single_process()->NeedsStartHostHook(*env_));
}

TEST_F(HostTypeTest, NeedsStartHostHook_SingleProcess_MultiProcessUnitActive) {
  env_->SetUnitActive(kMultiProcessUnit);

  EXPECT_TRUE(single_process()->NeedsStartHostHook(*env_));
}

TEST_F(HostTypeTest, NeedsStartHostHook_MultiProcess) {
  EXPECT_TRUE(gdm_managed()->NeedsStartHostHook(*env_));
}

TEST_F(HostTypeTest, IsUserNameSafeForUnitName) {
  EXPECT_TRUE(IsUserNameSafeForUnitName("alice"));
  EXPECT_TRUE(IsUserNameSafeForUnitName("Alice_B.c-1"));
  EXPECT_TRUE(IsUserNameSafeForUnitName("alice@example.com"));
  EXPECT_TRUE(IsUserNameSafeForUnitName("DOMAIN:alice"));
  EXPECT_TRUE(IsUserNameSafeForUnitName(std::string(200, 'a')));

  EXPECT_FALSE(IsUserNameSafeForUnitName(""));
  EXPECT_FALSE(IsUserNameSafeForUnitName("-alice"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("user/name"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("alice bob"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("alice\n"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("DOMAIN\\alice"));
  // systemctl treats these as glob patterns in unit names.
  EXPECT_FALSE(IsUserNameSafeForUnitName("*"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("alice?"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("[ab]"));
  // Not allowed in systemd unit names.
  EXPECT_FALSE(IsUserNameSafeForUnitName("alice$"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("alice;bob"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("alice'"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("alice%"));
  EXPECT_FALSE(IsUserNameSafeForUnitName("al\u00efce"));
  // Too long for a unit name with the prefix and suffix.
  EXPECT_FALSE(IsUserNameSafeForUnitName(std::string(201, 'a')));
}

TEST_F(HostTypeTest, CheckLocalUserForHostTypeSetup_UserGiven_Succeeds) {
  EXPECT_THAT(
      CheckLocalUserForHostTypeSetup(*single_process(), nullptr, kUserName),
      HasValue());
  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*gdm_managed(), single_process(),
                                             kUserName),
              HasValue());
  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*gdm_managed(), gdm_managed(),
                                             "alice@example.com"),
              HasValue());
}

TEST_F(HostTypeTest,
       CheckLocalUserForHostTypeSetup_RequiredByTargetButMissing_Fails) {
  EXPECT_THAT(
      CheckLocalUserForHostTypeSetup(*single_process(), gdm_managed(), ""),
      ErrorIs("A local user is required, since the single-process host type "
              "needs one."));
}

TEST_F(HostTypeTest, CheckLocalUserForHostTypeSetup_NotRequired_Succeeds) {
  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*gdm_managed(), nullptr, ""),
              HasValue());
  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*gdm_managed(), gdm_managed(), ""),
              HasValue());
}

TEST_F(HostTypeTest,
       CheckLocalUserForHostTypeSetup_MissingWithSingleProcessCurrent_Fails) {
  EXPECT_THAT(
      CheckLocalUserForHostTypeSetup(*gdm_managed(), single_process(), ""),
      ErrorIs("A local user is required, since the configured single-process "
              "host belongs to a local user."));
}

TEST_F(HostTypeTest, CheckLocalUserForHostTypeSetup_UnsafeName_Fails) {
  // Also if the host type doesn't require a local user.
  EXPECT_THAT(
      CheckLocalUserForHostTypeSetup(*single_process(), nullptr, "user name"),
      ErrorIs("The username 'user name' is not supported."));
  EXPECT_THAT(
      CheckLocalUserForHostTypeSetup(*gdm_managed(), nullptr, "DOMAIN\\u"),
      ErrorIs(HasSubstr("is not supported")));
}

TEST_F(HostTypeTest, GdmManagedStopHook_SucceedsWithoutRoutingFile) {
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetUnitEnabled(kMultiProcessUnit);

  EXPECT_TRUE(gdm_managed()->RunStopHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_FALSE(base::PathExists(backup_path_));
}

TEST_F(HostTypeTest, GdmManagedStopHook_OverwritesExistingBackup) {
  WriteRoutingConfig("{}");
  ASSERT_TRUE(base::WriteFile(backup_path_, "old"));

  EXPECT_TRUE(gdm_managed()->RunStopHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_EQ(ReadContent(backup_path_), "{}");
}

}  // namespace
}  // namespace remoting
