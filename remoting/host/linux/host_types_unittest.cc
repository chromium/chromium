// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_types.h"

#include <unistd.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "remoting/host/linux/host_type_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::IsEmpty;

constexpr char kUserName[] = "testuser";
constexpr char kSingleProcessUnit[] = "chrome-remote-desktop@testuser.service";
constexpr char kMultiProcessUnit[] = "chrome-remote-desktop.service";

class FakeHostTypeEnvironment : public HostTypeEnvironment {
 public:
  explicit FakeHostTypeEnvironment(const base::FilePath& routing_config_path)
      : routing_config_path_(routing_config_path) {}
  FakeHostTypeEnvironment(const FakeHostTypeEnvironment&) = delete;
  FakeHostTypeEnvironment& operator=(const FakeHostTypeEnvironment&) = delete;
  ~FakeHostTypeEnvironment() override = default;

  void SetUnitActive(const std::string& unit) {
    SetUnitActiveState(unit, "active");
  }
  // Units without an active state are "inactive".
  void SetUnitActiveState(const std::string& unit, std::string_view state) {
    unit_active_states_.insert_or_assign(unit, std::string(state));
  }
  void SetUnitEnabled(const std::string& unit) { enabled_units_.insert(unit); }
  // Makes EnableAndStartUnit() fail for `unit` without changing its state.
  void SetEnableAndStartUnitFails(const std::string& unit) {
    enable_failing_units_.insert(unit);
  }
  // Makes DisableAndStopUnit() fail for `unit` without changing its state.
  void SetDisableAndStopUnitFails(const std::string& unit) {
    disable_failing_units_.insert(unit);
  }

  // The units that EnableAndStartUnit() and DisableAndStopUnit() were called
  // for, e.g. "enable foo.service".
  const std::vector<std::string>& unit_operations() const {
    return unit_operations_;
  }

  // HostTypeEnvironment implementation.
  std::string GetUnitActiveState(const std::string& unit) const override {
    auto it = unit_active_states_.find(unit);
    return it == unit_active_states_.end() ? "inactive" : it->second;
  }
  bool IsUnitEnabled(const std::string& unit) const override {
    return enabled_units_.contains(unit);
  }
  bool EnableAndStartUnit(const std::string& unit) override {
    unit_operations_.push_back("enable " + unit);
    if (enable_failing_units_.contains(unit)) {
      return false;
    }
    enabled_units_.insert(unit);
    SetUnitActive(unit);
    return true;
  }
  bool DisableAndStopUnit(const std::string& unit) override {
    unit_operations_.push_back("disable " + unit);
    if (disable_failing_units_.contains(unit)) {
      return false;
    }
    enabled_units_.erase(unit);
    unit_active_states_.erase(unit);
    return true;
  }
  bool HasEnabledOrActiveUnits(std::string_view unit_pattern) const override {
    for (const auto& [unit, state] : unit_active_states_) {
      if (base::MatchPattern(unit, unit_pattern) && IsUnitActive(unit)) {
        return true;
      }
    }
    for (const std::string& unit : enabled_units_) {
      if (base::MatchPattern(unit, unit_pattern)) {
        return true;
      }
    }
    return false;
  }
  base::FilePath GetSessionRoutingConfigPath() const override {
    return routing_config_path_;
  }
  uid_t GetExpectedOwnerUid() const override { return geteuid(); }

 private:
  base::FilePath routing_config_path_;
  base::flat_map<std::string, std::string> unit_active_states_;
  base::flat_set<std::string> enabled_units_;
  base::flat_set<std::string> enable_failing_units_;
  base::flat_set<std::string> disable_failing_units_;
  std::vector<std::string> unit_operations_;
};

// A host type that appends the calls of its unit and state methods to `log`.
class FakeHostType : public HostType {
 public:
  FakeHostType(std::string_view name, std::vector<std::string>& log)
      : name_(name), log_(log) {}
  ~FakeHostType() override = default;

  void set_fail_method(std::string_view method) { fail_method_ = method; }

  std::string_view name() const override { return name_; }
  std::string_view description() const override { return "Fake host type."; }
  bool is_multi_process() const override { return true; }
  bool requires_local_user() const override { return false; }
  bool IsCurrent(const HostTypeEnvironment& env,
                 std::string_view local_user_name) const override {
    return false;
  }

 protected:
  bool StartHostUnit(const HostTypeContext& context) const override {
    return Log("StartHostUnit");
  }
  bool StopHostUnit(const HostTypeContext& context) const override {
    return Log("StopHostUnit");
  }

  bool SetUpState(const HostTypeContext& context) const override {
    return Log("SetUpState");
  }
  bool CleanUpState(const HostTypeContext& context) const override {
    return Log("CleanUpState");
  }

 private:
  bool Log(std::string_view method) const {
    log_->push_back(base::StrCat({name_, ".", method}));
    return method != fail_method_;
  }

  std::string name_;
  raw_ref<std::vector<std::string>> log_;
  std::string fail_method_;
};

class HostTypeTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    routing_config_path_ = temp_dir_.GetPath().Append("session_routing.json");
    backup_path_ = temp_dir_.GetPath().Append("session_routing.json.bak");
    env_.emplace(routing_config_path_);
  }

  void WriteRoutingConfig(std::string_view content) {
    ASSERT_TRUE(base::WriteFile(routing_config_path_, content));
    ASSERT_TRUE(base::SetPosixFilePermissions(routing_config_path_, 0644));
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

TEST_F(HostTypeTest, DetectCurrent_NothingActive_ReturnsNull) {
  // Enabled units and a routing file are leftovers if no host unit is active.
  env_->SetUnitEnabled(kSingleProcessUnit);
  env_->SetUnitEnabled(kMultiProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), nullptr);
  EXPECT_EQ(HostType::DetectCurrent(*env_, ""), nullptr);
}

TEST_F(HostTypeTest, DetectCurrent_FailedOrStoppingUnits_ReturnsNull) {
  env_->SetUnitActiveState(kSingleProcessUnit, "failed");
  env_->SetUnitActiveState(kMultiProcessUnit, "deactivating");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), nullptr);
}

TEST_F(HostTypeTest, DetectCurrent_SingleProcessUnitActive) {
  env_->SetUnitActive(kSingleProcessUnit);

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), single_process());
}

TEST_F(HostTypeTest, DetectCurrent_SingleProcessUnitActivating) {
  env_->SetUnitActiveState(kSingleProcessUnit, "activating");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), single_process());
}

TEST_F(HostTypeTest, DetectCurrent_MultiProcessUnitActivating) {
  env_->SetUnitActiveState(kMultiProcessUnit, "activating");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
}

TEST_F(HostTypeTest, DetectCurrent_SingleProcessUnitTakesPrecedence) {
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetUnitActive(kMultiProcessUnit);

  // Both types match, but single-process comes first in the detection order.
  EXPECT_TRUE(single_process()->IsCurrent(*env_, kUserName));
  EXPECT_TRUE(gdm_managed()->IsCurrent(*env_, kUserName));
  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), single_process());
}

TEST_F(HostTypeTest, DetectCurrent_NoUserName_SingleProcessUnitNotChecked) {
  env_->SetUnitActive("chrome-remote-desktop@.service");

  EXPECT_FALSE(single_process()->IsCurrent(*env_, ""));
  EXPECT_EQ(HostType::DetectCurrent(*env_, ""), nullptr);
}

TEST_F(HostTypeTest, DetectCurrent_OtherUsersSingleProcessUnitIgnored) {
  env_->SetUnitActive("chrome-remote-desktop@otheruser.service");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), nullptr);

  env_->SetUnitActive(kMultiProcessUnit);

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
}

TEST_F(HostTypeTest, DetectCurrent_MultiProcessUnitActive_ReturnsGdmManaged) {
  env_->SetUnitActive(kMultiProcessUnit);

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
  EXPECT_EQ(HostType::DetectCurrent(*env_, ""), gdm_managed());
}

TEST_F(HostTypeTest, DetectCurrent_RoutingDirectoryAbsent_ReturnsGdmManaged) {
  FakeHostTypeEnvironment env(
      temp_dir_.GetPath().Append("missing").Append("session_routing.json"));
  env.SetUnitActive(kMultiProcessUnit);

  EXPECT_EQ(HostType::DetectCurrent(env, kUserName), gdm_managed());
}

// gdm-managed is the only multi-process host type for now, so it also matches
// a running multi-process host with a routing file.
TEST_F(HostTypeTest, DetectCurrent_RoutingFile_InterimGdmManaged) {
  env_->SetUnitActive(kMultiProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
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
  EXPECT_FALSE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(), IsEmpty());
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

TEST_F(HostTypeTest, GdmManagedStartHook_NoUser_SkipsSingleProcessUnit) {
  // The template unit, which would be used if the empty user wasn't skipped.
  env_->SetUnitActive("chrome-remote-desktop@.service");
  env_->SetUnitEnabled("chrome-remote-desktop@.service");

  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"enable ", kMultiProcessUnit})));
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

TEST_F(HostTypeTest, SetUpAndStartHost_SingleProcessKept_OnlyStartsHostUnit) {
  // Would be stopped by the start hook of the single-process host type.
  env_->SetUnitEnabled(kMultiProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_TRUE(single_process()->SetUpAndStartHost(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false),
      /*keep_existing_setup=*/true));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"enable ", kSingleProcessUnit})));
  EXPECT_TRUE(env_->IsUnitActive(kSingleProcessUnit));
  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
}

TEST_F(HostTypeTest, Hooks_OrderOfUnitAndStateMethods) {
  std::vector<std::string> log;
  FakeHostType type("fake", log);
  HostTypeContext context(*env_, "", /*is_corp_host=*/false);

  EXPECT_TRUE(type.RunStartHostHook(context));
  EXPECT_TRUE(type.RunStopHostHook(context));

  EXPECT_THAT(log, ElementsAre("fake.SetUpState", "fake.StartHostUnit",
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

  EXPECT_THAT(log, ElementsAre("fake.SetUpState", "fake.StopHostUnit"));
}

TEST_F(HostTypeTest, SetUpAndStartHost_KeepExistingSetup_OnlyStartsHostUnit) {
  std::vector<std::string> log;
  FakeHostType type("fake", log);

  EXPECT_TRUE(
      type.SetUpAndStartHost(HostTypeContext(*env_, "", /*is_corp_host=*/false),
                             /*keep_existing_setup=*/true));

  EXPECT_THAT(log, ElementsAre("fake.StartHostUnit"));
}

TEST_F(HostTypeTest, SetUpAndStartHost_NewSetup_RunsStartHook) {
  std::vector<std::string> log;
  FakeHostType type("fake", log);

  EXPECT_TRUE(
      type.SetUpAndStartHost(HostTypeContext(*env_, "", /*is_corp_host=*/false),
                             /*keep_existing_setup=*/false));

  EXPECT_THAT(log, ElementsAre("fake.SetUpState", "fake.StartHostUnit"));
}

TEST_F(HostTypeTest, SetUpAndStartHost_StartHookFails_ReturnsFalse) {
  std::vector<std::string> log;
  FakeHostType type("fake", log);
  type.set_fail_method("StartHostUnit");

  EXPECT_FALSE(
      type.SetUpAndStartHost(HostTypeContext(*env_, "", /*is_corp_host=*/false),
                             /*keep_existing_setup=*/false));

  EXPECT_THAT(log, ElementsAre("fake.SetUpState", "fake.StartHostUnit"));
}

TEST_F(HostTypeTest,
       SetUpAndStartHost_KeepExistingSetup_StartFails_ReturnsFalse) {
  std::vector<std::string> log;
  FakeHostType type("fake", log);
  type.set_fail_method("StartHostUnit");

  EXPECT_FALSE(
      type.SetUpAndStartHost(HostTypeContext(*env_, "", /*is_corp_host=*/false),
                             /*keep_existing_setup=*/true));

  EXPECT_THAT(log, ElementsAre("fake.StartHostUnit"));
}

TEST_F(HostTypeTest, SetUpAndStartHost_GdmManaged_StopsSingleProcessUnit) {
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetUnitEnabled(kSingleProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_TRUE(gdm_managed()->SetUpAndStartHost(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false),
      /*keep_existing_setup=*/false));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kSingleProcessUnit}),
                          base::StrCat({"enable ", kMultiProcessUnit})));
  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_EQ(ReadContent(backup_path_), "{}");
}

TEST_F(HostTypeTest, SetUpAndStartHost_SingleProcess_StopsMultiProcessUnit) {
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetUnitEnabled(kMultiProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_TRUE(single_process()->SetUpAndStartHost(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false),
      /*keep_existing_setup=*/false));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kMultiProcessUnit}),
                          base::StrCat({"enable ", kSingleProcessUnit})));
  // The single-process host doesn't use the routing file.
  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
}

TEST_F(HostTypeTest, SetUpAndStartHost_GdmManagedKept_KeepsRoutingFile) {
  WriteRoutingConfig("{}");

  EXPECT_TRUE(gdm_managed()->SetUpAndStartHost(
      HostTypeContext(*env_, "", /*is_corp_host=*/false),
      /*keep_existing_setup=*/true));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"enable ", kMultiProcessUnit})));
  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
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

TEST_F(HostTypeTest, HasPerUserUnits_SingleProcess) {
  EXPECT_FALSE(single_process()->HasPerUserUnits(*env_));

  // The multi-process host unit is not a per-user unit.
  env_->SetUnitActive(kMultiProcessUnit);
  EXPECT_FALSE(single_process()->HasPerUserUnits(*env_));

  // Any user's unit counts, enabled or active.
  env_->SetUnitEnabled("chrome-remote-desktop@otheruser.service");
  EXPECT_TRUE(single_process()->HasPerUserUnits(*env_));
}

TEST_F(HostTypeTest, HasPerUserUnits_SingleProcessUnitActive) {
  env_->SetUnitActive(kSingleProcessUnit);

  EXPECT_TRUE(single_process()->HasPerUserUnits(*env_));
}

TEST_F(HostTypeTest, HasPerUserUnits_GdmManaged_AlwaysFalse) {
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetUnitActive(kMultiProcessUnit);

  EXPECT_FALSE(gdm_managed()->HasPerUserUnits(*env_));
}

TEST_F(HostTypeTest, CheckLocalUserForHostTypeSetup_UserGiven_Succeeds) {
  env_->SetUnitActive(kSingleProcessUnit);

  EXPECT_THAT(
      CheckLocalUserForHostTypeSetup(*env_, *single_process(), kUserName),
      HasValue());
  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*env_, *gdm_managed(), kUserName),
              HasValue());
  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*env_, *gdm_managed(),
                                             "alice@example.com"),
              HasValue());
}

TEST_F(HostTypeTest,
       CheckLocalUserForHostTypeSetup_RequiredByTargetButMissing_Fails) {
  EXPECT_THAT(
      CheckLocalUserForHostTypeSetup(*env_, *single_process(), ""),
      ErrorIs("A local user is required, since the single-process host type "
              "needs one."));
}

TEST_F(HostTypeTest,
       CheckLocalUserForHostTypeSetup_NotRequiredAndNoPerUserUnits_Succeeds) {
  env_->SetUnitActive(kMultiProcessUnit);

  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*env_, *gdm_managed(), ""),
              HasValue());
}

TEST_F(HostTypeTest,
       CheckLocalUserForHostTypeSetup_MissingWithPerUserUnits_Fails) {
  env_->SetUnitEnabled("chrome-remote-desktop@otheruser.service");

  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*env_, *gdm_managed(), ""),
              ErrorIs("A local user is required, since a per-user Chrome "
                      "Remote Desktop unit is enabled or active."));
}

TEST_F(HostTypeTest, CheckLocalUserForHostTypeSetup_UnsafeName_Fails) {
  // Also if the host type doesn't require a local user.
  EXPECT_THAT(
      CheckLocalUserForHostTypeSetup(*env_, *single_process(), "user name"),
      ErrorIs("The username 'user name' is not supported."));
  EXPECT_THAT(
      CheckLocalUserForHostTypeSetup(*env_, *gdm_managed(), "DOMAIN\\u"),
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
