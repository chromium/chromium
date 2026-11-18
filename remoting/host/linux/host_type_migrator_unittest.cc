// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_type_migrator.h"

#include <unistd.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/host/host_config.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/linux/host_types_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using testing::Contains;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Not;
using testing::StartsWith;

constexpr char kUserName[] = "testuser";
constexpr char kSingleProcessUnit[] = "chrome-remote-desktop@testuser.service";
constexpr char kMultiProcessUnit[] = "chrome-remote-desktop.service";
constexpr char kMigrateHost[] =
    "/opt/google/chrome-remote-desktop/migrate-host";
constexpr char kStartHost[] = "/opt/google/chrome-remote-desktop/start-host";

std::string RetryAndRestoreCommands(std::string_view current,
                                    std::string_view target,
                                    bool process_model_changes = false,
                                    bool keep_old_config = false,
                                    std::string_view user_name = kUserName) {
  std::string user_arg =
      user_name.empty() ? "" : base::StrCat({" --user-name=", user_name});
  return base::StrCat(
      {"The migration from ",
       current,
       " to ",
       target,
       " was not completed and has not been rolled back. To retry it, run:\n  ",
       kMigrateHost,
       " ",
       target,
       user_arg,
       keep_old_config ? " --keep-config" : "",
       "\n",
       process_model_changes
           ? base::StrCat(
                 {"Retrying may keep the host config and pairings of the ",
                  current, " host.\n"})
           : "",
       "To restore the ",
       current,
       " host instead, run:\n  ",
       kMigrateHost,
       " ",
       current,
       user_arg,
       "\n"});
}

std::string_view ProcessModelName(bool multi_process) {
  return multi_process ? "multi" : "single";
}

base::ListValue MakePairings(std::string_view client_name) {
  return base::ListValue().Append(
      base::DictValue()
          .Set("clientId", "11111111-2222-3333-4444-555555555555")
          .Set("clientName", client_name)
          .Set("createdTime", 1000.0)
          .Set("sharedSecret", "secret"));
}

class TestHostTypeMigrator : public HostTypeMigrator {
 public:
  using HostTypeMigrator::HostRegistration;
  using RegistrationResult =
      base::expected<std::optional<HostRegistration>, std::string>;

  TestHostTypeMigrator(FakeHostTypeEnvironment& env,
                       std::vector<std::string>& log,
                       const HostType* current_host_type,
                       const HostType& target_host_type,
                       const PasswdUserInfo* local_user,
                       bool keep_old_config = false)
      : HostTypeMigrator(env,
                         current_host_type,
                         target_host_type,
                         local_user,
                         keep_old_config),
        fake_env_(env),
        log_(log) {
    for (bool multi_process : {false, true}) {
      set_registration(
          multi_process,
          HostRegistration(base::DictValue().Set(kHostIdConfigPath, "host-id"),
                           MakePairings(base::StrCat(
                               {ProcessModelName(multi_process), "-client"}))));
    }
  }
  ~TestHostTypeMigrator() override = default;

  void set_registration(bool multi_process, RegistrationResult registration) {
    registrations_[multi_process] = std::move(registration);
  }

  void set_host_config(bool multi_process, base::DictValue host_config) {
    set_registration(
        multi_process,
        HostRegistration(std::move(host_config),
                         MakePairings(base::StrCat(
                             {ProcessModelName(multi_process), "-client"}))));
  }

  void set_fail_method(std::string_view method) { fail_method_ = method; }

  const std::optional<HostRegistration>& saved_registration() const {
    return saved_registration_;
  }

 protected:
  RegistrationResult LoadRegistration(bool multi_process) override {
    log_->push_back(base::StrCat(
        {"LoadRegistration(", ProcessModelName(multi_process), ")"}));
    const RegistrationResult& registration = registrations_[multi_process];
    if (!registration.has_value()) {
      return base::unexpected(registration.error());
    }
    if (!registration->has_value()) {
      return std::nullopt;
    }
    return HostRegistration((*registration)->host_config.Clone(),
                            (*registration)->pairings.Clone());
  }

  bool SaveRegistration(bool multi_process,
                        const HostRegistration& registration) override {
    saved_registration_.emplace(registration.host_config.Clone(),
                                registration.pairings.Clone());
    if (!LogWithUnitStates(base::StrCat(
            {"SaveRegistration(", ProcessModelName(multi_process), ")"}))) {
      return false;
    }
    if (multi_process) {
      EXPECT_TRUE(
          base::WriteFile(fake_env_->GetMultiProcessConfigPath(), "{}"));
    }
    return true;
  }

  bool DeleteRegistration(bool multi_process) override {
    return LogWithUnitStates(base::StrCat(
        {"DeleteRegistration(", ProcessModelName(multi_process), ")"}));
  }

 private:
  bool LogWithUnitStates(std::string_view call) {
    log_->push_back(
        base::StrCat({call, " single=", UnitState(kSingleProcessUnit),
                      " multi=", UnitState(kMultiProcessUnit)}));
    return fail_method_.empty() || !call.starts_with(fail_method_);
  }

  std::string_view UnitState(const std::string& unit) const {
    return fake_env_->IsUnitActive(unit) ? "active" : "inactive";
  }

  const raw_ref<const FakeHostTypeEnvironment> fake_env_;
  const raw_ref<std::vector<std::string>> log_;
  std::array<RegistrationResult, 2> registrations_;
  std::optional<HostRegistration> saved_registration_;
  std::string fail_method_;
};

class ContextRecordingHostType : public FakeHostType {
 public:
  using FakeHostType::FakeHostType;

  bool ValidateStartHostHookContext(
      const HostTypeContext& context) const override {
    local_user_name_ = context.local_user_name;
    is_corp_host_ = context.is_corp_host;
    return FakeHostType::ValidateStartHostHookContext(context);
  }

  const std::optional<std::string>& local_user_name() const {
    return local_user_name_;
  }
  std::optional<bool> is_corp_host() const { return is_corp_host_; }

 private:
  mutable std::optional<std::string> local_user_name_;
  mutable std::optional<bool> is_corp_host_;
};

class ReadHostConfigTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    config_path_ = temp_dir_.GetPath().Append("host.json");
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath config_path_;
};

TEST_F(ReadHostConfigTest, MissingOrEmptyHostId_ReturnsNoHost) {
  EXPECT_THAT(ReadHostConfig(config_path_), ValueIs(std::nullopt));
  for (std::string_view content :
       {R"({})", R"({"host_id": ""})", R"({"host_owner": "user@gmail.com"})"}) {
    SCOPED_TRACE(content);
    ASSERT_TRUE(base::WriteFile(config_path_, content));
    EXPECT_THAT(ReadHostConfig(config_path_), ValueIs(std::nullopt));
  }
}

TEST_F(ReadHostConfigTest, ValidConfig_ReturnsConfig) {
  ASSERT_TRUE(base::WriteFile(config_path_, R"({"host_id": "some-host-id"})"));
  auto config = ReadHostConfig(config_path_);
  ASSERT_TRUE(config.has_value());
  ASSERT_TRUE(config->has_value());
  EXPECT_EQ(**config, base::DictValue().Set(kHostIdConfigPath, "some-host-id"));
}

TEST_F(ReadHostConfigTest, MalformedJsonOrNonDictOrDirectory_ReturnsError) {
  for (std::string_view content :
       {"", "not json", R"({"host_id": "trunca)", "[]"}) {
    SCOPED_TRACE(content);
    ASSERT_TRUE(base::WriteFile(config_path_, content));
    EXPECT_THAT(ReadHostConfig(config_path_),
                ErrorIs(HasSubstr(config_path_.value())));
  }
  base::DeleteFile(config_path_);
  ASSERT_TRUE(base::CreateDirectory(config_path_));
  EXPECT_THAT(ReadHostConfig(config_path_),
              ErrorIs(HasSubstr(config_path_.value())));
}

TEST_F(ReadHostConfigTest, InaccessibleFileOrDirectory_ReturnsError) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "root can access any file/directory.";
  }
  ASSERT_TRUE(base::WriteFile(config_path_, R"({"host_id": "some-host-id"})"));
  ASSERT_TRUE(base::SetPosixFilePermissions(config_path_, 0));
  EXPECT_THAT(ReadHostConfig(config_path_),
              ErrorIs(HasSubstr(config_path_.value())));

  base::FilePath dir = temp_dir_.GetPath().Append("home");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::FilePath path = dir.Append("host.json");
  ASSERT_TRUE(base::WriteFile(path, R"({"host_id": "some-host-id"})"));
  ASSERT_TRUE(base::SetPosixFilePermissions(dir, 0));
  EXPECT_THAT(ReadHostConfig(path), ErrorIs(HasSubstr(path.value())));
  ASSERT_TRUE(base::SetPosixFilePermissions(dir, 0700));
}

class HostTypeMigratorTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    UseEnvironment(temp_dir_.GetPath().Append("session_routing.json"));
  }

  void UseEnvironment(const base::FilePath& routing_config_path) {
    routing_config_path_ = routing_config_path;
    env_.emplace(routing_config_path_);
  }

  void UseInsecureEnvironment() {
    base::FilePath directory = temp_dir_.GetPath().Append("insecure");
    ASSERT_TRUE(base::CreateDirectory(directory));
    ASSERT_TRUE(base::SetPosixFilePermissions(directory, 0777));
    UseEnvironment(directory.Append("session_routing.json"));
  }

  TestHostTypeMigrator CreateMigrator(const HostType* current,
                                      const HostType& target,
                                      std::string_view user_name = kUserName,
                                      bool keep_old_config = false) {
    const PasswdUserInfo* local_user = nullptr;
    if (!user_name.empty()) {
      user_info_ = PasswdUserInfo{
          .username = std::string(user_name),
          .uid = 1000,
          .gid = 1000,
          .home_dir = temp_dir_.GetPath().Append("home").Append(user_name),
      };
      local_user = &user_info_;
    }
    return TestHostTypeMigrator(*env_, log_, current, target, local_user,
                                keep_old_config);
  }

  bool Migrate(const HostType* current,
               const HostType& target,
               std::string_view user_name = kUserName,
               bool keep_old_config = false) {
    return CreateMigrator(current, target, user_name, keep_old_config).Run();
  }

  std::optional<std::string> ReadContent(const base::FilePath& path) {
    std::string content;
    return base::ReadFileToString(path, &content) ? std::optional(content)
                                                  : std::nullopt;
  }

  std::optional<std::string> ReadRecord() {
    return ReadContent(env_->GetHostTypeRecordPath());
  }

  const HostType* single_process() { return HostType::Find("single-process"); }
  const HostType* gdm_managed() { return HostType::Find("gdm-managed"); }

  base::ScopedTempDir temp_dir_;
  base::FilePath routing_config_path_;
  std::vector<std::string> log_;
  std::optional<FakeHostTypeEnvironment> env_;
  PasswdUserInfo user_info_;
};

TEST_F(HostTypeMigratorTest, SingleToMulti_CopiesStateBeforeStoppingCurrent) {
  env_->EnableAndStartUnit(kSingleProcessUnit);
  base::DictValue host_config =
      base::DictValue().Set(kHostIdConfigPath, "single-process-host-id");
  TestHostTypeMigrator migrator =
      CreateMigrator(single_process(), *gdm_managed());
  migrator.set_host_config(/*multi_process=*/false, host_config.Clone());

  testing::internal::CaptureStdout();
  EXPECT_TRUE(migrator.Run());
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_THAT(
      log_,
      ElementsAre("LoadRegistration(single)",
                  "SaveRegistration(multi) single=active multi=inactive",
                  "DeleteRegistration(single) single=inactive multi=active"));
  ASSERT_TRUE(migrator.saved_registration().has_value());
  EXPECT_EQ(migrator.saved_registration()->host_config, host_config);
  EXPECT_EQ(migrator.saved_registration()->pairings,
            MakePairings("single-client"));
  EXPECT_FALSE(env_->IsUnitEnabled(kSingleProcessUnit));
  EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
  EXPECT_THAT(HostType::GetConfigured(*env_), ValueIs(gdm_managed()));
  EXPECT_THAT(output, HasSubstr("Successfully migrated to gdm-managed host."));
  EXPECT_THAT(
      output,
      HasSubstr("If the connection doesn't work as expected, try rebooting the "
                "machine."));
}

TEST_F(HostTypeMigratorTest, MultiToSingle_CopiesStateAndDeletesRoutingConfig) {
  env_->EnableAndStartUnit(kMultiProcessUnit);
  ASSERT_TRUE(base::WriteFile(routing_config_path_, "{}"));
  base::DictValue host_config =
      base::DictValue().Set(kHostIdConfigPath, "multi-process-host-id");
  TestHostTypeMigrator migrator =
      CreateMigrator(gdm_managed(), *single_process());
  migrator.set_host_config(/*multi_process=*/true, host_config.Clone());

  EXPECT_TRUE(migrator.Run());

  EXPECT_THAT(
      log_,
      ElementsAre("LoadRegistration(multi)",
                  "SaveRegistration(single) single=inactive multi=active",
                  "DeleteRegistration(multi) single=active multi=inactive"));
  ASSERT_TRUE(migrator.saved_registration().has_value());
  EXPECT_EQ(migrator.saved_registration()->host_config, host_config);
  EXPECT_FALSE(env_->IsUnitEnabled(kMultiProcessUnit));
  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_TRUE(env_->IsUnitActive(kSingleProcessUnit));
  EXPECT_THAT(HostType::GetConfigured(*env_), ValueIs(single_process()));
}

TEST_F(HostTypeMigratorTest,
       SameProcessModel_NeitherCopiesNorDeletesRegistration) {
  FakeHostType current("current", log_);
  FakeHostType target("target", log_);

  EXPECT_TRUE(Migrate(&current, target));

  EXPECT_THAT(log_, ElementsAre("LoadRegistration(multi)",
                                "target.ValidateStartHostHookContext",
                                "current.StopHostUnit", "current.CleanUpState",
                                "target.ValidateStartHostHookContext",
                                "target.SetUpState", "target.StartHostUnit"));
  EXPECT_EQ(ReadRecord(), R"({"hostType":"target"})");
}

TEST_F(HostTypeMigratorTest, KeepConfig_PreservesOldRegistration) {
  env_->EnableAndStartUnit(kSingleProcessUnit);

  testing::internal::CaptureStdout();
  EXPECT_TRUE(Migrate(single_process(), *gdm_managed(), kUserName,
                      /*keep_old_config=*/true));
  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_THAT(
      log_,
      ElementsAre("LoadRegistration(single)",
                  "SaveRegistration(multi) single=active multi=inactive"));
  EXPECT_THAT(HostType::GetConfigured(*env_), ValueIs(gdm_managed()));
  EXPECT_THAT(output, HasSubstr("Keeping old host config as requested."));

  // In same-process-model, --keep-config prints nothing.
  FakeHostType current("current", log_);
  FakeHostType target("target", log_);
  testing::internal::CaptureStdout();
  EXPECT_TRUE(Migrate(&current, target, kUserName, /*keep_old_config=*/true));
  EXPECT_THAT(testing::internal::GetCapturedStdout(),
              Not(HasSubstr("Keeping old host config")));
}

TEST_F(HostTypeMigratorTest,
       DeleteRegistrationFails_ReportsMigrationSucceededWithCleanupError) {
  env_->EnableAndStartUnit(kSingleProcessUnit);
  TestHostTypeMigrator migrator =
      CreateMigrator(single_process(), *gdm_managed());
  migrator.set_fail_method("DeleteRegistration");

  testing::internal::CaptureStderr();
  EXPECT_FALSE(migrator.Run());
  std::string error_output = testing::internal::GetCapturedStderr();

  EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
  EXPECT_THAT(HostType::GetConfigured(*env_), ValueIs(gdm_managed()));
  EXPECT_THAT(error_output,
              HasSubstr("The migration to gdm-managed succeeded, but cleaning "
                        "up the old host config of the single-process host "
                        "failed."));
}

TEST_F(HostTypeMigratorTest,
       ValidationOrSaveFailure_LeavesCurrentHostUntouched) {
  // Pre-flight context validation failure: single_process requires a valid
  // user.
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetUnitEnabled(kMultiProcessUnit);
  testing::internal::CaptureStderr();
  EXPECT_FALSE(Migrate(gdm_managed(), *single_process(), "-unsafe"));
  EXPECT_THAT(testing::internal::GetCapturedStderr(),
              HasSubstr("The gdm-managed host was not changed."));
  EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
  EXPECT_EQ(ReadRecord(), R"({"hostType":"gdm-managed"})");

  // SaveRegistration failure.
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetUnitEnabled(kSingleProcessUnit);
  base::DeleteFile(env_->GetHostTypeRecordPath());
  TestHostTypeMigrator migrator =
      CreateMigrator(single_process(), *gdm_managed());
  migrator.set_fail_method("SaveRegistration");
  testing::internal::CaptureStderr();
  EXPECT_FALSE(migrator.Run());
  EXPECT_THAT(testing::internal::GetCapturedStderr(),
              HasSubstr("The single-process host was not changed."));
  EXPECT_TRUE(env_->IsUnitActive(kSingleProcessUnit));
  EXPECT_EQ(ReadRecord(), R"({"hostType":"single-process"})");
}

TEST_F(HostTypeMigratorTest,
       StopOrStartHookFailure_KeepsOldConfigAndPrintsRetryAndRestore) {
  // Stop hook failure.
  FakeHostType current("current", log_);
  current.set_fail_method("CleanUpState");
  FakeHostType target("target", log_);

  testing::internal::CaptureStderr();
  EXPECT_FALSE(Migrate(&current, target));
  EXPECT_THAT(testing::internal::GetCapturedStderr(),
              HasSubstr(RetryAndRestoreCommands("current", "target")));
  EXPECT_EQ(ReadRecord(), R"({"hostType":"current"})");

  // Stop hook failure with keep_old_config = true and empty user_name.
  current.set_fail_method("CleanUpState");
  testing::internal::CaptureStderr();
  EXPECT_FALSE(Migrate(&current, target, /*user_name=*/"",
                       /*keep_old_config=*/true));
  EXPECT_THAT(testing::internal::GetCapturedStderr(),
              HasSubstr(RetryAndRestoreCommands(
                  "current", "target", /*process_model_changes=*/false,
                  /*keep_old_config=*/true, /*user_name=*/"")));

  // Start hook failure.
  env_->EnableAndStartUnit(kMultiProcessUnit);
  env_->SetEnableAndStartUnitFails(kSingleProcessUnit);
  testing::internal::CaptureStderr();
  EXPECT_FALSE(Migrate(gdm_managed(), *single_process()));
  EXPECT_THAT(
      testing::internal::GetCapturedStderr(),
      HasSubstr(RetryAndRestoreCommands("gdm-managed", "single-process",
                                        /*process_model_changes=*/true)));
  EXPECT_THAT(log_, Not(Contains(StartsWith("DeleteRegistration"))));
}

TEST_F(HostTypeMigratorTest, NoHostTypeConfigured_PrintsStartHost) {
  testing::internal::CaptureStderr();
  EXPECT_FALSE(Migrate(nullptr, *gdm_managed()));
  EXPECT_THAT(testing::internal::GetCapturedStderr(),
              HasSubstr(base::StrCat({"No host is set up. To set up a ",
                                      "gdm-managed host, run:\n  ", kStartHost,
                                      " --host-type=gdm-managed\n"})));
}

TEST_F(HostTypeMigratorTest, NoHostConfig_SameType_PrintsStartHost) {
  env_->SetUnitActive(kMultiProcessUnit);
  TestHostTypeMigrator migrator = CreateMigrator(gdm_managed(), *gdm_managed());
  migrator.set_registration(/*multi_process=*/true, std::nullopt);

  testing::internal::CaptureStderr();
  EXPECT_FALSE(migrator.Run());
  EXPECT_THAT(testing::internal::GetCapturedStderr(),
              HasSubstr("The configured host type is already gdm-managed, but "
                        "no host is set up."));
  EXPECT_THAT(env_->unit_operations(), IsEmpty());
}

TEST_F(HostTypeMigratorTest,
       NoHostConfig_DifferentType_SwitchesRecordAndCleansLeftoverUnit) {
  FakeHostType current("current", log_);
  ContextRecordingHostType target("target", log_);
  TestHostTypeMigrator migrator = CreateMigrator(&current, target);
  migrator.set_registration(/*multi_process=*/true, std::nullopt);

  testing::internal::CaptureStdout();
  EXPECT_TRUE(migrator.Run());
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(ReadRecord(), R"({"hostType":"target"})");
  EXPECT_EQ(target.is_corp_host(), true);
  EXPECT_THAT(output,
              AllOf(HasSubstr("start-host"), HasSubstr("--host-type=target")));

  // Leftover unit of single-process host is disabled without starting target.
  env_->SetUnitEnabled(kSingleProcessUnit);
  TestHostTypeMigrator migrator2 =
      CreateMigrator(single_process(), *gdm_managed());
  migrator2.set_registration(/*multi_process=*/false, std::nullopt);
  EXPECT_TRUE(migrator2.Run());
  EXPECT_FALSE(env_->IsUnitEnabled(kSingleProcessUnit));
  EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
  EXPECT_THAT(HostType::GetConfigured(*env_), ValueIs(gdm_managed()));
}

TEST_F(HostTypeMigratorTest,
       NoHostConfig_DifferentType_FailureKeepsCurrentType) {
  // Invalid context: target single_process fails validation for -unsafe.
  env_->SetUnitEnabled(kMultiProcessUnit);
  TestHostTypeMigrator migrator =
      CreateMigrator(gdm_managed(), *single_process(), "-unsafe");
  migrator.set_registration(/*multi_process=*/true, std::nullopt);
  EXPECT_FALSE(migrator.Run());
  EXPECT_EQ(ReadRecord(), R"({"hostType":"gdm-managed"})");

  // Stop hook fails.
  base::DeleteFile(env_->GetHostTypeRecordPath());
  FakeHostType current("current", log_);
  current.set_fail_method("CleanUpState");
  FakeHostType target("target", log_);
  TestHostTypeMigrator migrator2 = CreateMigrator(&current, target);
  migrator2.set_registration(/*multi_process=*/true, std::nullopt);
  EXPECT_FALSE(migrator2.Run());
  EXPECT_EQ(ReadRecord(), R"({"hostType":"current"})");

  // Record fails in insecure directory.
  UseInsecureEnvironment();
  ASSERT_TRUE(base::WriteFile(env_->GetHostTypeRecordPath(),
                              R"({"hostType":"current"})"));
  FakeHostType current2("current", log_);
  FakeHostType target2("target", log_);
  TestHostTypeMigrator migrator3 = CreateMigrator(&current2, target2);
  migrator3.set_registration(/*multi_process=*/true, std::nullopt);
  EXPECT_FALSE(migrator3.Run());
  EXPECT_EQ(ReadRecord(), R"({"hostType":"current"})");
}

TEST_F(HostTypeMigratorTest, HostConfigReadError_ChangesNothing) {
  env_->SetUnitActive(kSingleProcessUnit);
  TestHostTypeMigrator migrator =
      CreateMigrator(single_process(), *gdm_managed());
  migrator.set_registration(/*multi_process=*/false,
                            base::unexpected("Failed to read the config"));

  testing::internal::CaptureStderr();
  EXPECT_FALSE(migrator.Run());
  EXPECT_THAT(testing::internal::GetCapturedStderr(),
              HasSubstr("Failed to read the config"));
  EXPECT_THAT(env_->unit_operations(), IsEmpty());
  EXPECT_EQ(ReadRecord(), std::nullopt);
}

TEST_F(HostTypeMigratorTest, ContextPropagation_CorpAndNonCorp) {
  FakeHostType current("current", log_);
  ContextRecordingHostType target("target", log_);

  // Non-corp config.
  EXPECT_TRUE(Migrate(&current, target));
  EXPECT_EQ(target.local_user_name(), kUserName);
  EXPECT_EQ(target.is_corp_host(), false);

  // Corp config.
  TestHostTypeMigrator corp_migrator = CreateMigrator(&current, target, "");
  corp_migrator.set_host_config(
      /*multi_process=*/true, base::DictValue()
                                  .Set(kHostIdConfigPath, "host-id")
                                  .Set(kHostTypeHintPath, kCorpHostTypeHint));
  EXPECT_TRUE(corp_migrator.Run());
  EXPECT_EQ(target.local_user_name(), "");
  EXPECT_EQ(target.is_corp_host(), true);
}

TEST_F(HostTypeMigratorTest, Repair_RestartsUnitAndRerunsStartHook) {
  ASSERT_TRUE(gdm_managed()->WriteHostTypeRecord(*env_));
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetUnitEnabled(kMultiProcessUnit);

  testing::internal::CaptureStdout();
  EXPECT_TRUE(Migrate(gdm_managed(), *gdm_managed()));
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_THAT(output,
              HasSubstr("Successfully set up the gdm-managed host again."));
  EXPECT_THAT(
      output,
      HasSubstr("If the connection doesn't work as expected, try rebooting the "
                "machine."));
  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kMultiProcessUnit}),
                          base::StrCat({"enable ", kMultiProcessUnit})));

  // Repair stops unit without CleanUpState.
  log_.clear();
  base::DeleteFile(env_->GetHostTypeRecordPath());
  FakeHostType type("fake", log_);
  EXPECT_TRUE(Migrate(&type, type));
  EXPECT_THAT(log_, ElementsAre("LoadRegistration(multi)",
                                "fake.ValidateStartHostHookContext",
                                "fake.StopHostUnit",
                                "fake.ValidateStartHostHookContext",
                                "fake.SetUpState", "fake.StartHostUnit"));

  // Failure prints retry command (no restore command).
  type.set_fail_method("StartHostUnit");
  testing::internal::CaptureStderr();
  EXPECT_FALSE(Migrate(&type, type));
  std::string err = testing::internal::GetCapturedStderr();
  EXPECT_THAT(err, HasSubstr(base::StrCat({"To retry, run:\n  ", kMigrateHost,
                                           " fake --user-name=testuser\n"})));
  EXPECT_THAT(err, Not(HasSubstr("restore")));

  // Validation failure leaves active unit running.
  env_->SetUnitActive(kMultiProcessUnit);
  type.set_fail_method("ValidateStartHostHookContext");
  testing::internal::CaptureStderr();
  EXPECT_FALSE(Migrate(&type, type));
  EXPECT_THAT(testing::internal::GetCapturedStderr(),
              HasSubstr("Cannot set up the fake host again. The host was not "
                        "changed."));
  EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
}

TEST_F(HostTypeMigratorTest,
       PreflightRecordFailure_NoRecordFile_LeavesCurrentHostUntouched) {
  env_->EnableAndStartUnit(kMultiProcessUnit);
  base::DeleteFile(env_->GetHostTypeRecordPath());
  base::FilePath record_dir = env_->GetHostTypeRecordPath().DirName();
  ASSERT_TRUE(base::SetPosixFilePermissions(record_dir, 0777));

  testing::internal::CaptureStderr();
  EXPECT_FALSE(Migrate(gdm_managed(), *single_process()));
  std::string error_output = testing::internal::GetCapturedStderr();

  EXPECT_THAT(error_output,
              HasSubstr("Failed to record the configured host type "
                        "(gdm-managed). Nothing was changed."));
  EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
  EXPECT_FALSE(base::PathExists(env_->GetHostTypeRecordPath()));

  ASSERT_TRUE(base::SetPosixFilePermissions(record_dir, 0755));
}

}  // namespace
}  // namespace remoting
