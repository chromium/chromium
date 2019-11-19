// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/win/sandbox_win.h"

#include <algorithm>
#include <vector>

#include <windows.h>

#include <sddl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/app_container_profile_base.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sid.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {

namespace {

constexpr wchar_t kBaseSecurityDescriptor[] = L"D:(A;;GA;;;WD)";
constexpr char kAppContainerId[] = "SandboxWinTest";
constexpr wchar_t kPackageSid[] =
    L"S-1-15-2-2739114418-4250112400-4176314265-1208402406-1880724913-"
    L"3756377648-2708578895";
constexpr wchar_t kChromeInstallFiles[] = L"chromeInstallFiles";
constexpr wchar_t kLpacChromeInstallFiles[] = L"lpacChromeInstallFiles";
constexpr wchar_t kRegistryRead[] = L"registryRead";

class TestTargetPolicy : public sandbox::TargetPolicy {
 public:
  void AddRef() override {}
  void Release() override {}
  sandbox::ResultCode SetTokenLevel(sandbox::TokenLevel initial,
                                    sandbox::TokenLevel lockdown) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::TokenLevel GetInitialTokenLevel() const override {
    return sandbox::TokenLevel{};
  }
  sandbox::TokenLevel GetLockdownTokenLevel() const override {
    return sandbox::TokenLevel{};
  }
  sandbox::ResultCode SetJobLevel(sandbox::JobLevel job_level,
                                  uint32_t ui_exceptions) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::JobLevel GetJobLevel() const override { return sandbox::JobLevel{}; }
  sandbox::ResultCode SetJobMemoryLimit(size_t memory_limit) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode SetAlternateDesktop(bool alternate_winstation) override {
    return sandbox::SBOX_ALL_OK;
  }
  base::string16 GetAlternateDesktop() const override {
    return base::string16();
  }
  sandbox::ResultCode CreateAlternateDesktop(
      bool alternate_winstation) override {
    return sandbox::SBOX_ALL_OK;
  }
  void DestroyAlternateDesktop() override {}
  sandbox::ResultCode SetIntegrityLevel(
      sandbox::IntegrityLevel level) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::IntegrityLevel GetIntegrityLevel() const override {
    return sandbox::IntegrityLevel{};
  }
  sandbox::ResultCode SetDelayedIntegrityLevel(
      sandbox::IntegrityLevel level) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode SetLowBox(const wchar_t* sid) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode SetProcessMitigations(
      sandbox::MitigationFlags flags) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::MitigationFlags GetProcessMitigations() override {
    return sandbox::MitigationFlags{};
  }
  sandbox::ResultCode SetDelayedProcessMitigations(
      sandbox::MitigationFlags flags) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::MitigationFlags GetDelayedProcessMitigations() const override {
    return sandbox::MitigationFlags{};
  }
  sandbox::ResultCode SetDisconnectCsrss() override {
    return sandbox::SBOX_ALL_OK;
  }
  void SetStrictInterceptions() override {}
  sandbox::ResultCode SetStdoutHandle(HANDLE handle) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode SetStderrHandle(HANDLE handle) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode AddRule(SubSystem subsystem,
                              Semantics semantics,
                              const wchar_t* pattern) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode AddDllToUnload(const wchar_t* dll_name) override {
    return sandbox::SBOX_ALL_OK;
  }
  sandbox::ResultCode AddKernelObjectToClose(
      const wchar_t* handle_type,
      const wchar_t* handle_name) override {
    return sandbox::SBOX_ALL_OK;
  }
  void AddHandleToShare(HANDLE handle) override {}
  void SetLockdownDefaultDacl() override {}
  void SetEnableOPMRedirection() override {}
  bool GetEnableOPMRedirection() override { return false; }

  sandbox::ResultCode AddAppContainerProfile(const wchar_t* package_name,
                                             bool create_profile) override {
    if (create_profile) {
      app_container_profile_ = sandbox::AppContainerProfileBase::Create(
          package_name, L"Sandbox", L"Sandbox");
    } else {
      app_container_profile_ =
          sandbox::AppContainerProfileBase::Open(package_name);
    }
    if (!app_container_profile_)
      return sandbox::SBOX_ERROR_CREATE_APPCONTAINER_PROFILE;
    return sandbox::SBOX_ALL_OK;
  }

  scoped_refptr<sandbox::AppContainerProfile> GetAppContainerProfile()
      override {
    return app_container_profile_;
  }

  scoped_refptr<sandbox::AppContainerProfileBase> GetAppContainerProfileBase() {
    return app_container_profile_;
  }

  void SetEffectiveToken(HANDLE token) override {}

 private:
  scoped_refptr<sandbox::AppContainerProfileBase> app_container_profile_;
};

std::vector<sandbox::Sid> GetCapabilitySids(
    const std::initializer_list<base::string16>& capabilities) {
  std::vector<sandbox::Sid> sids;
  for (const auto& capability : capabilities) {
    sids.emplace_back(sandbox::Sid::FromNamedCapability(capability.c_str()));
  }
  return sids;
}

base::string16 GetAccessAllowedForCapabilities(
    const std::initializer_list<base::string16>& capabilities) {
  base::string16 sddl = kBaseSecurityDescriptor;
  for (const auto& capability : GetCapabilitySids(capabilities)) {
    base::string16 sid_string;
    CHECK(capability.ToSddlString(&sid_string));
    base::StrAppend(&sddl, {L"(A;;GRGX;;;", sid_string, L")"});
  }
  return sddl;
}

// Drops a temporary file granting RX access to a list of capabilities.
bool DropTempFileWithSecurity(
    const base::ScopedTempDir& temp_dir,
    const std::initializer_list<base::string16>& capabilities,
    base::FilePath* path) {
  if (!base::CreateTemporaryFileInDir(temp_dir.GetPath(), path))
    return false;
  auto sddl = GetAccessAllowedForCapabilities(capabilities);
  PSECURITY_DESCRIPTOR security_descriptor = nullptr;
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(
          sddl.c_str(), SDDL_REVISION_1, &security_descriptor, nullptr)) {
    return false;
  }
  BOOL result = ::SetFileSecurityW(
      path->value().c_str(), DACL_SECURITY_INFORMATION, security_descriptor);
  ::LocalFree(security_descriptor);
  return !!result;
}

bool EqualSidList(const std::vector<sandbox::Sid>& left,
                  const std::vector<sandbox::Sid>& right) {
  if (left.size() != right.size())
    return false;
  auto result = std::mismatch(left.cbegin(), left.cend(), right.cbegin(),
                              [](const auto& left_sid, const auto& right_sid) {
                                return !!::EqualSid(left_sid.GetPSID(),
                                                    right_sid.GetPSID());
                              });
  return result.first == left.cend();
}

bool CheckCapabilities(
    sandbox::AppContainerProfileBase* profile,
    const std::initializer_list<base::string16>& additional_capabilities) {
  auto additional_caps = GetCapabilitySids(additional_capabilities);
  auto impersonation_caps = GetCapabilitySids(
      {kChromeInstallFiles, kLpacChromeInstallFiles, kRegistryRead});
  auto base_caps = GetCapabilitySids({kLpacChromeInstallFiles, kRegistryRead});

  impersonation_caps.insert(impersonation_caps.end(), additional_caps.begin(),
                            additional_caps.end());
  base_caps.insert(base_caps.end(), additional_caps.begin(),
                   additional_caps.end());

  return EqualSidList(impersonation_caps,
                      profile->GetImpersonationCapabilities()) &&
         EqualSidList(base_caps, profile->GetCapabilities());
}

class SandboxWinTest : public ::testing::Test {
 public:
  SandboxWinTest() {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {}

 protected:
  void CreateProgramFile(std::initializer_list<base::string16> capabilities,
                         base::CommandLine* command_line) {
    base::FilePath path;
    ASSERT_TRUE(DropTempFileWithSecurity(temp_dir_, capabilities, &path));
    command_line->SetProgram(path);
  }

  sandbox::ResultCode CreateAppContainerProfile(
      const base::CommandLine& base_command_line,
      bool access_check_fail,
      service_manager::SandboxType sandbox_type,
      scoped_refptr<sandbox::AppContainerProfileBase>* profile) {
    base::FilePath path;
    base::CommandLine command_line(base_command_line);

    if (access_check_fail) {
      CreateProgramFile({}, &command_line);
    } else {
      CreateProgramFile({kChromeInstallFiles, kLpacChromeInstallFiles},
                        &command_line);
    }

    TestTargetPolicy policy;
    sandbox::ResultCode result =
        service_manager::SandboxWin::AddAppContainerProfileToPolicy(
            command_line, sandbox_type, kAppContainerId, &policy);
    if (result == sandbox::SBOX_ALL_OK)
      *profile = policy.GetAppContainerProfileBase();
    return result;
  }

  base::ScopedTempDir temp_dir_;
};

}  // namespace

TEST_F(SandboxWinTest, IsGpuAppContainerEnabled) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(service_manager::SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, SANDBOX_TYPE_GPU));
  command_line.AppendSwitch(switches::kEnableGpuAppContainer);
  EXPECT_TRUE(service_manager::SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, SANDBOX_TYPE_GPU));
  EXPECT_FALSE(service_manager::SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, SANDBOX_TYPE_NO_SANDBOX));
  command_line.AppendSwitch(switches::kDisableGpuAppContainer);
  EXPECT_FALSE(service_manager::SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, SANDBOX_TYPE_GPU));
}

TEST_F(SandboxWinTest, AppContainerAccessCheckFail) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  scoped_refptr<sandbox::AppContainerProfileBase> profile;
  sandbox::ResultCode result =
      CreateAppContainerProfile(command_line, true, SANDBOX_TYPE_GPU, &profile);
  EXPECT_EQ(sandbox::SBOX_ERROR_CREATE_APPCONTAINER_PROFILE_ACCESS_CHECK,
            result);
  EXPECT_EQ(nullptr, profile);
}

TEST_F(SandboxWinTest, AppContainerCheckProfile) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  scoped_refptr<sandbox::AppContainerProfileBase> profile;
  sandbox::ResultCode result = CreateAppContainerProfile(
      command_line, false, SANDBOX_TYPE_GPU, &profile);
  ASSERT_EQ(sandbox::SBOX_ALL_OK, result);
  ASSERT_NE(nullptr, profile);
  auto package_sid = sandbox::Sid::FromSddlString(kPackageSid);
  ASSERT_TRUE(package_sid.IsValid());
  EXPECT_TRUE(
      ::EqualSid(package_sid.GetPSID(), profile->GetPackageSid().GetPSID()));
  EXPECT_TRUE(profile->GetEnableLowPrivilegeAppContainer());
  EXPECT_TRUE(CheckCapabilities(profile.get(), {}));
}

TEST_F(SandboxWinTest, AppContainerCheckProfileDisableLpac) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kDisableGpuLpac);
  scoped_refptr<sandbox::AppContainerProfileBase> profile;
  sandbox::ResultCode result = CreateAppContainerProfile(
      command_line, false, SANDBOX_TYPE_GPU, &profile);
  ASSERT_EQ(sandbox::SBOX_ALL_OK, result);
  ASSERT_NE(nullptr, profile);
  EXPECT_FALSE(profile->GetEnableLowPrivilegeAppContainer());
}

TEST_F(SandboxWinTest, AppContainerCheckProfileAddCapabilities) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAddGpuAppContainerCaps,
                                 "  cap1   ,   cap2   ,");
  scoped_refptr<sandbox::AppContainerProfileBase> profile;
  sandbox::ResultCode result = CreateAppContainerProfile(
      command_line, false, SANDBOX_TYPE_GPU, &profile);
  ASSERT_EQ(sandbox::SBOX_ALL_OK, result);
  ASSERT_NE(nullptr, profile);
  EXPECT_TRUE(CheckCapabilities(profile.get(), {L"cap1", L"cap2"}));
}

}  // namespace service_manager
