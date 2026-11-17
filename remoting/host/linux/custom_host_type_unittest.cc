// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/custom_host_type.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/linux/host_types_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

using base::test::ErrorIs;
using base::test::IsJson;
using base::test::ValueIs;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Optional;

constexpr char kUserName[] = "testuser";
constexpr char kSingleProcessUnit[] = "chrome-remote-desktop@testuser.service";
constexpr char kMultiProcessUnit[] = "chrome-remote-desktop.service";
// A user name from SSSD, which is not a valid `sessionUser`.
constexpr char kDomainUserName[] = "user@domain";

class CustomHostTypeTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    routing_config_path_ = temp_dir_.GetPath().Append("session_routing.json");
    env_.emplace(routing_config_path_);
    record_path_ = env_->GetHostTypeRecordPath();
  }

  void WriteRoutingConfig(std::string_view content) {
    WriteFileWithMode(routing_config_path_, content);
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

  const HostType* custom() { return HostType::Find("custom"); }

  base::ScopedTempDir temp_dir_;
  base::FilePath routing_config_path_;
  base::FilePath record_path_;
  std::optional<FakeHostTypeEnvironment> env_;
};

TEST_F(CustomHostTypeTest, Properties) {
  ASSERT_NE(custom(), nullptr);
  EXPECT_TRUE(custom()->is_multi_process());
  EXPECT_TRUE(custom()->requires_local_user());
  EXPECT_EQ(custom(), &GetCustomHostType());
  EXPECT_EQ(custom()->name(), "custom");
}

TEST_F(CustomHostTypeTest, CustomStartHook_WritesRoutingFileAndStartsUnit) {
  EXPECT_TRUE(custom()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(ReadContent(routing_config_path_),
              Optional(IsJson(R"({"sessionUser": "testuser"})")));
  EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
  EXPECT_THAT(HostType::GetConfigured(*env_), ValueIs(custom()));
}

TEST_F(CustomHostTypeTest, CustomStartHook_CorpHost_OmitsSessionUser) {
  EXPECT_TRUE(custom()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/true)));

  EXPECT_THAT(ReadContent(routing_config_path_), Optional(IsJson("{}")));
  EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
}

TEST_F(CustomHostTypeTest, CustomStartHook_CorpHost_AllowsDomainUser) {
  EXPECT_TRUE(custom()->ValidateStartHostHookContext(
      HostTypeContext(*env_, kDomainUserName, /*is_corp_host=*/true)));
  EXPECT_TRUE(custom()->RunStartHostHook(
      HostTypeContext(*env_, kDomainUserName, /*is_corp_host=*/true)));

  EXPECT_THAT(ReadContent(routing_config_path_), Optional(IsJson("{}")));
  EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
}

TEST_F(CustomHostTypeTest, CustomStartHook_OverwritesExistingRoutingFile) {
  for (std::string_view content : {
           "hand-written",
           R"({"sessionUser": "testuser", "createRemoteUserSessions": true})",
       }) {
    SCOPED_TRACE(content);
    WriteRoutingConfig(content);

    EXPECT_TRUE(custom()->RunStartHostHook(
        HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

    EXPECT_THAT(ReadContent(routing_config_path_),
                Optional(IsJson(R"({"sessionUser": "testuser"})")));
    EXPECT_TRUE(env_->IsUnitActive(kMultiProcessUnit));
  }
}

TEST_F(CustomHostTypeTest, CustomStartHook_AlreadySetUp_NonCorpToCorp) {
  env_->EnableAndStartUnit(kMultiProcessUnit);
  WriteRoutingConfig(R"({"sessionUser": "testuser"})");

  EXPECT_TRUE(custom()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/true)));

  EXPECT_THAT(ReadContent(routing_config_path_), Optional(IsJson("{}")));
}

TEST_F(CustomHostTypeTest, CustomStartHook_RejectsUnsupportedUser) {
  for (const char* user_name : {"", kDomainUserName}) {
    SCOPED_TRACE(user_name);
    EXPECT_FALSE(custom()->ValidateStartHostHookContext(
        HostTypeContext(*env_, user_name, /*is_corp_host=*/false)));
    EXPECT_FALSE(custom()->RunStartHostHook(
        HostTypeContext(*env_, user_name, /*is_corp_host=*/false)));
  }

  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_FALSE(base::PathExists(record_path_));
  EXPECT_THAT(env_->unit_operations(), IsEmpty());
}

TEST_F(CustomHostTypeTest, CustomStartHook_StopsSingleProcessUnits) {
  env_->SetUnitEnabled(kSingleProcessUnit);

  EXPECT_TRUE(custom()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kSingleProcessUnit}),
                          base::StrCat({"enable ", kMultiProcessUnit})));
}

TEST_F(CustomHostTypeTest,
       CustomStartHook_FailsIfSingleProcessUnitFailsToStop) {
  env_->SetUnitActive(kSingleProcessUnit);
  env_->SetDisableAndStopUnitFails(kSingleProcessUnit);

  EXPECT_FALSE(custom()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
}

TEST_F(CustomHostTypeTest, CustomStopHook_StopsUnitAndDeletesRoutingFile) {
  for (bool is_corp_host : {false, true}) {
    SCOPED_TRACE(is_corp_host);
    env_->SetUnitActive(kMultiProcessUnit);
    env_->SetUnitEnabled(kMultiProcessUnit);
    WriteRoutingConfig(R"({"sessionUser": "testuser"})");

    EXPECT_TRUE(custom()->RunStopHostHook(
        HostTypeContext(*env_, kUserName, is_corp_host)));

    EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
    EXPECT_FALSE(env_->IsUnitEnabled(kMultiProcessUnit));
    EXPECT_FALSE(base::PathExists(routing_config_path_));
  }
}

TEST_F(CustomHostTypeTest, CustomStopHook_SucceedsWithoutRoutingFile) {
  env_->SetUnitActive(kMultiProcessUnit);

  EXPECT_TRUE(custom()->RunStopHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
  EXPECT_FALSE(base::PathExists(routing_config_path_));
}

TEST_F(CustomHostTypeTest, CustomStopHook_KeepsRoutingFileIfUnitFailsToStop) {
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetDisableAndStopUnitFails(kMultiProcessUnit);
  WriteRoutingConfig(R"({"sessionUser": "testuser"})");

  EXPECT_FALSE(custom()->RunStopHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_EQ(ReadContent(routing_config_path_),
            R"({"sessionUser": "testuser"})");
}

TEST_F(CustomHostTypeTest, CheckLocalUserForHostTypeSetup_CustomRequiresUser) {
  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*custom(), nullptr, ""),
              ErrorIs("A local user is required, since the custom host type "
                      "needs one."));
}

}  // namespace
}  // namespace remoting
