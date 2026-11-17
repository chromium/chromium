// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/auto_session_host_type.h"

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
constexpr char kX11SessionUnit[] =
    "chrome-remote-desktop-x11-session@testuser.service";
constexpr char kOtherX11SessionUnit[] =
    "chrome-remote-desktop-x11-session@otheruser.service";
constexpr char kDomainUserName[] = "user@domain";

class AutoSessionHostTypeTest : public testing::Test {
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

  const HostType* auto_session() { return HostType::Find("auto-session"); }

  base::ScopedTempDir temp_dir_;
  base::FilePath routing_config_path_;
  base::FilePath record_path_;
  std::optional<FakeHostTypeEnvironment> env_;
};

TEST_F(AutoSessionHostTypeTest, AutoSessionProperties) {
  ASSERT_NE(auto_session(), nullptr);
  EXPECT_TRUE(auto_session()->is_multi_process());
  EXPECT_TRUE(auto_session()->requires_local_user());
}

TEST_F(AutoSessionHostTypeTest,
       AutoSessionStartHook_WritesRoutingFileAndStartsHost) {
  EXPECT_TRUE(auto_session()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(
      ReadContent(routing_config_path_),
      Optional(IsJson(
          R"({"sessionUser": "testuser", "createRemoteUserSessions": true})")));
  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"enable ", kMultiProcessUnit})));
  EXPECT_THAT(HostType::GetConfigured(*env_), ValueIs(auto_session()));
}

TEST_F(AutoSessionHostTypeTest,
       AutoSessionStartHook_CorpHost_OmitsSessionUser) {
  EXPECT_TRUE(auto_session()->RunStartHostHook(
      HostTypeContext(*env_, kDomainUserName, /*is_corp_host=*/true)));

  EXPECT_THAT(ReadContent(routing_config_path_),
              Optional(IsJson(R"({"createRemoteUserSessions": true})")));
}

TEST_F(AutoSessionHostTypeTest,
       AutoSessionStartHook_OverwritesExistingRoutingFile) {
  for (std::string_view content :
       {"hand-written", R"({"sessionUser": "testuser"})"}) {
    SCOPED_TRACE(content);
    env_->EnableAndStartUnit(kMultiProcessUnit);
    WriteRoutingConfig(content);

    EXPECT_TRUE(auto_session()->RunStartHostHook(
        HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

    constexpr std::string_view kExpectedJson =
        R"({"sessionUser": "testuser", "createRemoteUserSessions": true})";
    EXPECT_THAT(ReadContent(routing_config_path_),
                Optional(IsJson(kExpectedJson)));
  }
}

TEST_F(AutoSessionHostTypeTest,
       AutoSessionStartHook_StopsSessionUnitsOfAllUsers) {
  env_->SetUnitEnabled(kX11SessionUnit);
  env_->SetUnitEnabled(kOtherX11SessionUnit);

  EXPECT_TRUE(auto_session()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_FALSE(env_->IsUnitEnabled(kX11SessionUnit));
  EXPECT_FALSE(env_->IsUnitEnabled(kOtherX11SessionUnit));
}

TEST_F(AutoSessionHostTypeTest, AutoSessionStartHook_StopsSingleProcessUnits) {
  env_->SetUnitEnabled("chrome-remote-desktop@otheruser.service");
  env_->SetUnitActive(kSingleProcessUnit);

  EXPECT_TRUE(auto_session()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre("disable chrome-remote-desktop@otheruser.service",
                          base::StrCat({"disable ", kSingleProcessUnit}),
                          base::StrCat({"enable ", kMultiProcessUnit})));
}

TEST_F(AutoSessionHostTypeTest, AutoSessionStartHook_RejectsUnsupportedUser) {
  for (const char* user_name : {"", kDomainUserName}) {
    SCOPED_TRACE(user_name);
    EXPECT_FALSE(auto_session()->ValidateStartHostHookContext(
        HostTypeContext(*env_, user_name, /*is_corp_host=*/false)));
    EXPECT_FALSE(auto_session()->RunStartHostHook(
        HostTypeContext(*env_, user_name, /*is_corp_host=*/false)));
  }

  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_FALSE(base::PathExists(record_path_));
  EXPECT_THAT(env_->unit_operations(), IsEmpty());
}

TEST_F(AutoSessionHostTypeTest,
       AutoSessionStopHook_StopsHostAndDeletesRoutingFile) {
  for (bool is_corp_host : {false, true}) {
    SCOPED_TRACE(is_corp_host);
    env_->SetUnitActive(kMultiProcessUnit);
    env_->SetUnitEnabled(kX11SessionUnit);
    WriteRoutingConfig(
        R"({"sessionUser": "testuser", "createRemoteUserSessions": true})");

    EXPECT_TRUE(auto_session()->RunStopHostHook(
        HostTypeContext(*env_, kUserName, is_corp_host)));

    EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
    // Session units are left alone.
    EXPECT_TRUE(env_->IsUnitEnabled(kX11SessionUnit));
    EXPECT_FALSE(base::PathExists(routing_config_path_));
  }
}

TEST_F(AutoSessionHostTypeTest,
       CheckLocalUserForHostTypeSetup_AutoSessionRequiresUser) {
  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*auto_session(), nullptr, ""),
              ErrorIs("A local user is required, since the auto-session host "
                      "type needs one."));
}

}  // namespace
}  // namespace remoting
