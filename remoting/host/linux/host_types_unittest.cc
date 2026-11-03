// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_types.h"

#include <unistd.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "remoting/host/linux/host_type_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

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

  void SetUnitActive(const std::string& unit) { active_units_.insert(unit); }

  // HostTypeEnvironment implementation.
  bool IsUnitActive(const std::string& unit) const override {
    return active_units_.contains(unit);
  }
  bool IsUnitEnabled(const std::string& unit) const override {
    return enabled_units_.contains(unit);
  }
  bool EnableAndStartUnit(const std::string& unit) override {
    enabled_units_.insert(unit);
    active_units_.insert(unit);
    return true;
  }
  bool DisableAndStopUnit(const std::string& unit) override {
    enabled_units_.erase(unit);
    active_units_.erase(unit);
    return true;
  }
  base::FilePath GetSessionRoutingConfigPath() const override {
    return routing_config_path_;
  }
  uid_t GetExpectedOwnerUid() const override { return geteuid(); }

 private:
  base::FilePath routing_config_path_;
  base::flat_set<std::string> active_units_;
  base::flat_set<std::string> enabled_units_;
};

class HostTypeTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    routing_config_path_ = temp_dir_.GetPath().Append("session_routing.json");
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
  std::optional<FakeHostTypeEnvironment> env_;
};

TEST_F(HostTypeTest, RequiresLocalUser) {
  EXPECT_TRUE(single_process()->requires_local_user());
  EXPECT_FALSE(gdm_managed()->requires_local_user());
}

TEST_F(HostTypeTest, DetectCurrent_NoServiceActive_ReturnsNull) {
  WriteRoutingConfig("{}");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), nullptr);
}

TEST_F(HostTypeTest, DetectCurrent_SingleProcessUnitActive) {
  env_->SetUnitActive(kSingleProcessUnit);

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), single_process());
}

TEST_F(HostTypeTest, DetectCurrent_SingleProcessUnitTakesPrecedence) {
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetUnitActive(kMultiProcessUnit);

  // Both types match, but single-process comes first in the detection order.
  EXPECT_TRUE(single_process()->IsCurrent(*env_, kUserName));
  EXPECT_TRUE(gdm_managed()->IsCurrent(*env_, kUserName));
  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), single_process());
}

TEST_F(HostTypeTest, IsCurrent_SingleProcess) {
  EXPECT_FALSE(single_process()->IsCurrent(*env_, kUserName));

  env_->SetUnitActive(kMultiProcessUnit);
  EXPECT_FALSE(single_process()->IsCurrent(*env_, kUserName));

  env_->SetUnitActive(kSingleProcessUnit);
  EXPECT_TRUE(single_process()->IsCurrent(*env_, kUserName));
  EXPECT_FALSE(single_process()->IsCurrent(*env_, "otheruser"));
  EXPECT_FALSE(single_process()->IsCurrent(*env_, ""));
}

TEST_F(HostTypeTest, IsCurrent_GdmManaged) {
  EXPECT_FALSE(gdm_managed()->IsCurrent(*env_, kUserName));

  env_->SetUnitActive(kSingleProcessUnit);
  EXPECT_FALSE(gdm_managed()->IsCurrent(*env_, kUserName));

  env_->SetUnitActive(kMultiProcessUnit);
  EXPECT_TRUE(gdm_managed()->IsCurrent(*env_, kUserName));
  EXPECT_TRUE(gdm_managed()->IsCurrent(*env_, ""));
}

TEST_F(HostTypeTest, DetectCurrent_NoUserName_SingleProcessUnitNotChecked) {
  env_->SetUnitActive("chrome-remote-desktop@.service");

  EXPECT_EQ(HostType::DetectCurrent(*env_, ""), nullptr);
}

TEST_F(HostTypeTest, DetectCurrent_OtherUsersSingleProcessUnitIgnored) {
  env_->SetUnitActive("chrome-remote-desktop@otheruser.service");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), nullptr);

  env_->SetUnitActive(kMultiProcessUnit);

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
}

TEST_F(HostTypeTest, DetectCurrent_RoutingFileAbsent_ReturnsGdmManaged) {
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

// The tests below cover the states that will be reported as the `custom` and
// `auto-session` host types once they exist. Until then, DetectCurrent() falls
// back to `gdm-managed`, the only multi-process host type.

TEST_F(HostTypeTest, DetectCurrent_EmptyRoutingFile_InterimGdmManaged) {
  env_->SetUnitActive(kMultiProcessUnit);
  WriteRoutingConfig("{}");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
}

TEST_F(HostTypeTest, DetectCurrent_CreateRemoteUserSessions_InterimGdmManaged) {
  env_->SetUnitActive(kMultiProcessUnit);
  WriteRoutingConfig(R"({"createRemoteUserSessions": true})");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
}

TEST_F(HostTypeTest, DetectCurrent_UnparsableRoutingFile_InterimGdmManaged) {
  env_->SetUnitActive(kMultiProcessUnit);
  WriteRoutingConfig("not json");

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
}

TEST_F(HostTypeTest, DetectCurrent_InsecureRoutingFile_InterimGdmManaged) {
  env_->SetUnitActive(kMultiProcessUnit);
  WriteRoutingConfig("{}");
  ASSERT_TRUE(base::SetPosixFilePermissions(routing_config_path_, 0666));

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
}

TEST_F(HostTypeTest, DetectCurrent_InsecureDirectory_InterimGdmManaged) {
  env_->SetUnitActive(kMultiProcessUnit);
  WriteRoutingConfig("{}");
  ASSERT_TRUE(base::SetPosixFilePermissions(temp_dir_.GetPath(), 0770));

  EXPECT_EQ(HostType::DetectCurrent(*env_, kUserName), gdm_managed());
}

TEST_F(HostTypeTest, GdmManagedStartHook_SucceedsIfRoutingFileAbsent) {
  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_FALSE(base::PathExists(routing_config_path_));
}

TEST_F(HostTypeTest, GdmManagedStartHook_SucceedsIfRoutingDirectoryAbsent) {
  FakeHostTypeEnvironment env(
      temp_dir_.GetPath().Append("missing").Append("session_routing.json"));

  EXPECT_TRUE(gdm_managed()->RunStartHostHook(
      HostTypeContext(env, "", /*is_corp_host=*/false)));
}

TEST_F(HostTypeTest, GdmManagedStartHook_FailsAndKeepsFileIfRoutingFileExists) {
  WriteRoutingConfig(R"({"sessionUser": "testuser"})");

  EXPECT_FALSE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_EQ(ReadContent(routing_config_path_),
            R"({"sessionUser": "testuser"})");
}

TEST_F(HostTypeTest, GdmManagedStartHook_FailsIfDanglingSymlink) {
  base::FilePath target = temp_dir_.GetPath().Append("missing.json");
  ASSERT_TRUE(base::CreateSymbolicLink(target, routing_config_path_));

  EXPECT_FALSE(gdm_managed()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_TRUE(base::IsLink(routing_config_path_));
  EXPECT_FALSE(base::PathExists(target));
}

TEST_F(HostTypeTest, GdmManagedStopHook_KeepsRoutingFile) {
  WriteRoutingConfig("{}");

  EXPECT_TRUE(gdm_managed()->RunStopHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
}

TEST_F(HostTypeTest, SingleProcessHooks_KeepRoutingFile) {
  WriteRoutingConfig("{}");
  HostTypeContext context(*env_, kUserName, /*is_corp_host=*/false);

  EXPECT_TRUE(single_process()->RunStartHostHook(context));
  EXPECT_TRUE(single_process()->RunStopHostHook(context));

  EXPECT_EQ(ReadContent(routing_config_path_), "{}");
}

}  // namespace
}  // namespace remoting
