// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/unit_session_host_type.h"

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
using testing::UnorderedElementsAre;

constexpr char kUserName[] = "testuser";
constexpr char kSingleProcessUnit[] = "chrome-remote-desktop@testuser.service";
constexpr char kMultiProcessUnit[] = "chrome-remote-desktop.service";
constexpr char kX11SessionUnit[] =
    "chrome-remote-desktop-x11-session@testuser.service";
constexpr char kOtherX11SessionUnit[] =
    "chrome-remote-desktop-x11-session@otheruser.service";
// A user name from SSSD, which is not a valid `sessionUser`.
constexpr char kDomainUserName[] = "user@domain";

class UnitSessionHostTypeTest : public testing::Test {
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

  const HostType* single_process() { return HostType::Find("single-process"); }
  const HostType* gdm_managed() { return HostType::Find("gdm-managed"); }
  const HostType* custom() { return HostType::Find("custom"); }
  const HostType* x11() { return HostType::Find("x11"); }

  base::ScopedTempDir temp_dir_;
  base::FilePath routing_config_path_;
  base::FilePath record_path_;
  std::optional<FakeHostTypeEnvironment> env_;
};

TEST_F(UnitSessionHostTypeTest, X11Properties) {
  ASSERT_NE(x11(), nullptr);
  EXPECT_TRUE(x11()->is_multi_process());
  EXPECT_TRUE(x11()->requires_local_user());
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_WritesRoutingFileAndStartsUnits) {
  EXPECT_TRUE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(ReadContent(routing_config_path_),
              Optional(IsJson(R"({"sessionUser": "testuser"})")));
  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"enable ", kX11SessionUnit}),
                          base::StrCat({"enable ", kMultiProcessUnit})));
  EXPECT_THAT(HostType::GetConfigured(*env_), ValueIs(x11()));
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_CorpHost_OmitsSessionUser) {
  EXPECT_TRUE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/true)));

  EXPECT_THAT(ReadContent(routing_config_path_), Optional(IsJson("{}")));
  EXPECT_TRUE(env_->IsUnitActive(kX11SessionUnit));
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_OverwritesExistingRoutingFile) {
  for (std::string_view content :
       {"hand-written", R"({"sessionUser": "otheruser"})"}) {
    SCOPED_TRACE(content);
    WriteRoutingConfig(content);

    EXPECT_TRUE(x11()->RunStartHostHook(
        HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

    EXPECT_THAT(ReadContent(routing_config_path_),
                Optional(IsJson(R"({"sessionUser": "testuser"})")));
  }
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_FailsWithoutUser) {
  EXPECT_FALSE(x11()->ValidateStartHostHookContext(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));
  EXPECT_FALSE(x11()->RunStartHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_FALSE(base::PathExists(record_path_));
  EXPECT_THAT(env_->unit_operations(), IsEmpty());
}

TEST_F(UnitSessionHostTypeTest,
       X11StartHook_DomainUser_OnlySupportedOnCorpHosts) {
  EXPECT_FALSE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kDomainUserName, /*is_corp_host=*/false)));
  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_FALSE(base::PathExists(record_path_));
  EXPECT_THAT(env_->unit_operations(), IsEmpty());

  EXPECT_TRUE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kDomainUserName, /*is_corp_host=*/true)));
  EXPECT_TRUE(env_->IsUnitActive(
      "chrome-remote-desktop-x11-session@user@domain.service"));
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_FailsIfSessionUnitFailsToStart) {
  env_->SetEnableAndStartUnitFails(kX11SessionUnit);

  EXPECT_FALSE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_FALSE(env_->IsUnitActive(kMultiProcessUnit));
  // Running the start hook again repairs the setup.
  EXPECT_THAT(HostType::GetConfigured(*env_), ValueIs(x11()));
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_StopsOtherUsersSessionUnits) {
  env_->SetUnitEnabled(kOtherX11SessionUnit);
  env_->SetUnitEnabled(kX11SessionUnit);

  EXPECT_TRUE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kOtherX11SessionUnit}),
                          base::StrCat({"enable ", kX11SessionUnit}),
                          base::StrCat({"enable ", kMultiProcessUnit})));
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_StopsSingleProcessUnits) {
  env_->SetUnitEnabled("chrome-remote-desktop@otheruser.service");
  env_->SetUnitActive(kSingleProcessUnit);

  EXPECT_TRUE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre("disable chrome-remote-desktop@otheruser.service",
                          base::StrCat({"disable ", kSingleProcessUnit}),
                          base::StrCat({"enable ", kX11SessionUnit}),
                          base::StrCat({"enable ", kMultiProcessUnit})));
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_FailsIfUnitsCannotBeListed) {
  env_->SetListUnitsFails();

  EXPECT_FALSE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_FALSE(base::PathExists(routing_config_path_));
  EXPECT_THAT(env_->unit_operations(), IsEmpty());
}

TEST_F(UnitSessionHostTypeTest,
       X11StartHook_AlreadySetUp_RestartsExitedSessionUnit) {
  env_->EnableAndStartUnit(kMultiProcessUnit);
  // The session has exited, e.g. because the user logged out.
  env_->SetUnitEnabled(kX11SessionUnit);
  WriteRoutingConfig(R"({"sessionUser": "testuser"})");

  EXPECT_TRUE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_TRUE(env_->IsUnitActive(kX11SessionUnit));
  EXPECT_THAT(ReadContent(routing_config_path_),
              Optional(IsJson(R"({"sessionUser": "testuser"})")));
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_AlreadySetUp_CorpToNonCorp) {
  env_->EnableAndStartUnit(kMultiProcessUnit);
  env_->EnableAndStartUnit(kX11SessionUnit);
  // Written by x11 on a corp host.
  WriteRoutingConfig("{}");

  EXPECT_TRUE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  // On non-corp hosts, connections are routed to the local user.
  EXPECT_THAT(ReadContent(routing_config_path_),
              Optional(IsJson(R"({"sessionUser": "testuser"})")));
  EXPECT_TRUE(env_->IsUnitActive(kX11SessionUnit));
}

TEST_F(UnitSessionHostTypeTest, X11StartHook_AlreadySetUp_NonCorpToCorp) {
  env_->EnableAndStartUnit(kMultiProcessUnit);
  env_->EnableAndStartUnit(kX11SessionUnit);
  // Written by x11 on a non-corp host.
  WriteRoutingConfig(R"({"sessionUser": "testuser"})");

  EXPECT_TRUE(x11()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/true)));

  EXPECT_THAT(ReadContent(routing_config_path_), Optional(IsJson("{}")));
}

TEST_F(UnitSessionHostTypeTest, X11StopHook_StopsUnitsAndDeletesRoutingFile) {
  for (bool is_corp_host : {false, true}) {
    SCOPED_TRACE(is_corp_host);
    env_->EnableAndStartUnit(kMultiProcessUnit);
    env_->EnableAndStartUnit(kX11SessionUnit);
    WriteRoutingConfig(R"({"sessionUser": "testuser"})");

    EXPECT_TRUE(x11()->RunStopHostHook(
        HostTypeContext(*env_, kUserName, is_corp_host)));

    EXPECT_FALSE(env_->IsUnitEnabled(kMultiProcessUnit));
    EXPECT_FALSE(env_->IsUnitEnabled(kX11SessionUnit));
    EXPECT_FALSE(base::PathExists(routing_config_path_));
  }
}

TEST_F(UnitSessionHostTypeTest,
       X11StopHook_StopsHostUnitBeforeSessionUnitsOfAllUsers) {
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetUnitEnabled(kOtherX11SessionUnit);

  // The local user isn't needed.
  EXPECT_TRUE(x11()->RunStopHostHook(
      HostTypeContext(*env_, "", /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kMultiProcessUnit}),
                          base::StrCat({"disable ", kOtherX11SessionUnit})));
}

TEST_F(UnitSessionHostTypeTest, X11StopHook_FailsIfUnitsCannotBeListed) {
  env_->SetListUnitsFails();
  WriteRoutingConfig(R"({"sessionUser": "testuser"})");

  EXPECT_FALSE(x11()->RunStopHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_EQ(ReadContent(routing_config_path_),
            R"({"sessionUser": "testuser"})");
}

TEST_F(UnitSessionHostTypeTest,
       X11StopHook_KeepsRoutingFileIfSessionUnitFailsToStop) {
  env_->SetUnitActive(kMultiProcessUnit);
  env_->SetUnitActive(kX11SessionUnit);
  env_->SetDisableAndStopUnitFails(kX11SessionUnit);
  WriteRoutingConfig(R"({"sessionUser": "testuser"})");

  EXPECT_FALSE(x11()->RunStopHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_EQ(ReadContent(routing_config_path_),
            R"({"sessionUser": "testuser"})");
}

TEST_F(UnitSessionHostTypeTest, MultiProcessStartHooks_SessionUnitCleanup) {
  for (const HostType* type : {gdm_managed(), custom()}) {
    SCOPED_TRACE(type->name());
    env_->SetUnitEnabled(kX11SessionUnit);
    env_->SetUnitEnabled(kOtherX11SessionUnit);

    EXPECT_TRUE(type->RunStartHostHook(
        HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

    EXPECT_FALSE(env_->IsUnitEnabled(kX11SessionUnit));
    EXPECT_FALSE(env_->IsUnitEnabled(kOtherX11SessionUnit));
  }
}

TEST_F(UnitSessionHostTypeTest, MultiProcessStopHooks_LeaveSessionUnitsAlone) {
  for (const HostType* type : {gdm_managed(), custom()}) {
    SCOPED_TRACE(type->name());
    env_->SetUnitEnabled(kX11SessionUnit);

    EXPECT_TRUE(type->RunStopHostHook(
        HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

    EXPECT_TRUE(env_->IsUnitEnabled(kX11SessionUnit));
  }
}

TEST_F(UnitSessionHostTypeTest,
       SingleProcessStartHook_StopsSessionUnitsOfAllUsers) {
  env_->SetUnitEnabled(kMultiProcessUnit);
  env_->SetUnitEnabled(kOtherX11SessionUnit);

  EXPECT_TRUE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_THAT(env_->unit_operations(),
              ElementsAre(base::StrCat({"disable ", kMultiProcessUnit}),
                          base::StrCat({"disable ", kOtherX11SessionUnit}),
                          base::StrCat({"enable ", kSingleProcessUnit})));
}

TEST_F(UnitSessionHostTypeTest,
       SingleProcessStartHook_FailsIfUnitsCannotBeListed) {
  env_->SetListUnitsFails();

  EXPECT_FALSE(single_process()->RunStartHostHook(
      HostTypeContext(*env_, kUserName, /*is_corp_host=*/false)));

  EXPECT_FALSE(env_->IsUnitEnabled(kSingleProcessUnit));
  EXPECT_THAT(env_->unit_operations(), IsEmpty());
}

TEST_F(UnitSessionHostTypeTest,
       CheckLocalUserForHostTypeSetup_X11RequiresUser) {
  EXPECT_THAT(CheckLocalUserForHostTypeSetup(*x11(), nullptr, ""),
              ErrorIs("A local user is required, since the x11 host type "
                      "needs one."));
}

TEST_F(UnitSessionHostTypeTest,
       StopAllSessionUnits_RegistersAndUnregistersInstances) {
  constexpr char kExtraSessionUnit[] = "extra-session@testuser.service";
  {
    UnitSessionHostType extra("extra", "Extra unit session host",
                              "extra-session@");
    env_->SetUnitEnabled(kX11SessionUnit);
    env_->SetUnitEnabled(kExtraSessionUnit);

    EXPECT_TRUE(UnitSessionHostType::StopAllSessionUnits(*env_));
    EXPECT_THAT(
        env_->unit_operations(),
        UnorderedElementsAre(base::StrCat({"disable ", kX11SessionUnit}),
                             base::StrCat({"disable ", kExtraSessionUnit})));
  }

  env_->SetUnitEnabled(kX11SessionUnit);
  env_->SetUnitEnabled(kExtraSessionUnit);

  EXPECT_TRUE(UnitSessionHostType::StopAllSessionUnits(*env_));
  EXPECT_FALSE(env_->IsUnitEnabled(kX11SessionUnit));
  EXPECT_TRUE(env_->IsUnitEnabled(kExtraSessionUnit));
}

}  // namespace
}  // namespace remoting
