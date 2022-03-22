// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_win.h"

#include <algorithm>
#include <string>
#include <vector>

#include <windows.h>

#include <sddl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/sandbox_test_utils.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_diagnostic.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace sandbox {
namespace policy {

namespace {
class TestTargetPolicy : public TargetPolicy {
 public:
  void AddRef() override {}
  void Release() override {}
  ResultCode SetTokenLevel(sandbox::TokenLevel initial,
                           TokenLevel lockdown) override {
    return SBOX_ALL_OK;
  }
  TokenLevel GetInitialTokenLevel() const override { return TokenLevel{}; }
  TokenLevel GetLockdownTokenLevel() const override { return TokenLevel{}; }
  ResultCode SetJobLevel(sandbox::JobLevel job_level,
                         uint32_t ui_exceptions) override {
    return SBOX_ALL_OK;
  }
  JobLevel GetJobLevel() const override { return sandbox::JobLevel{}; }
  ResultCode SetJobMemoryLimit(size_t memory_limit) override {
    return SBOX_ALL_OK;
  }
  ResultCode SetAlternateDesktop(bool alternate_winstation) override {
    return SBOX_ALL_OK;
  }
  std::wstring GetAlternateDesktop() const override { return std::wstring(); }
  ResultCode CreateAlternateDesktop(bool alternate_winstation) override {
    return SBOX_ALL_OK;
  }
  void DestroyAlternateDesktop() override {}
  ResultCode SetIntegrityLevel(IntegrityLevel level) override {
    return SBOX_ALL_OK;
  }
  IntegrityLevel GetIntegrityLevel() const override { return IntegrityLevel{}; }
  ResultCode SetDelayedIntegrityLevel(IntegrityLevel level) override {
    return SBOX_ALL_OK;
  }
  ResultCode SetLowBox(const wchar_t* sid) override { return SBOX_ALL_OK; }
  ResultCode SetProcessMitigations(MitigationFlags flags) override {
    return SBOX_ALL_OK;
  }
  MitigationFlags GetProcessMitigations() override { return MitigationFlags{}; }
  ResultCode SetDelayedProcessMitigations(MitigationFlags flags) override {
    return SBOX_ALL_OK;
  }
  MitigationFlags GetDelayedProcessMitigations() const override {
    return MitigationFlags{};
  }
  ResultCode SetDisconnectCsrss() override { return SBOX_ALL_OK; }
  void SetStrictInterceptions() override {}
  ResultCode SetStdoutHandle(HANDLE handle) override { return SBOX_ALL_OK; }
  ResultCode SetStderrHandle(HANDLE handle) override { return SBOX_ALL_OK; }
  ResultCode AddRule(SubSystem subsystem,
                     Semantics semantics,
                     const wchar_t* pattern) override {
    return SBOX_ALL_OK;
  }
  ResultCode AddDllToUnload(const wchar_t* dll_name) override {
    blocklisted_dlls_.push_back(dll_name);
    return SBOX_ALL_OK;
  }
  ResultCode AddKernelObjectToClose(const wchar_t* handle_type,
                                    const wchar_t* handle_name) override {
    return SBOX_ALL_OK;
  }
  void AddHandleToShare(HANDLE handle) override {}
  void SetLockdownDefaultDacl() override {}
  void AddRestrictingRandomSid() override {}

  ResultCode AddAppContainerProfile(const wchar_t* package_name,
                                    bool create_profile) override {
    if (create_profile) {
      app_container_ =
          AppContainerBase::CreateProfile(package_name, L"Sandbox", L"Sandbox");
    } else {
      app_container_ = AppContainerBase::Open(package_name);
    }
    if (!app_container_)
      return SBOX_ERROR_CREATE_APPCONTAINER;
    return SBOX_ALL_OK;
  }

  scoped_refptr<AppContainer> GetAppContainer() override {
    return app_container_;
  }

  scoped_refptr<AppContainerBase> GetAppContainerBase() {
    return app_container_;
  }

  void SetEffectiveToken(HANDLE token) override {}

  const std::vector<std::wstring>& blocklisted_dlls() const {
    return blocklisted_dlls_;
  }

  std::unique_ptr<PolicyInfo> GetPolicyInfo() override { return nullptr; }
  void SetAllowNoSandboxJob() override { NOTREACHED(); }
  bool GetAllowNoSandboxJob() override { return false; }

 private:
  std::vector<std::wstring> blocklisted_dlls_;
  scoped_refptr<AppContainerBase> app_container_;
};

// Drops a temporary file granting RX access to a list of capabilities.
bool DropTempFileWithSecurity(
    const base::ScopedTempDir& temp_dir,
    const std::initializer_list<std::wstring>& capabilities,
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

class SandboxWinTest : public ::testing::Test {
 public:
  SandboxWinTest() {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {}

 protected:
  void CreateProgramFile(std::initializer_list<std::wstring> capabilities,
                         base::CommandLine* command_line) {
    base::FilePath path;
    ASSERT_TRUE(DropTempFileWithSecurity(temp_dir_, capabilities, &path));
    command_line->SetProgram(path);
  }

  ResultCode CreateAppContainerProfile(
      const base::CommandLine& base_command_line,
      bool access_check_fail,
      sandbox::mojom::Sandbox sandbox_type,
      scoped_refptr<AppContainerBase>* profile) {
    base::FilePath path;
    base::CommandLine command_line(base_command_line);

    if (access_check_fail) {
      CreateProgramFile({}, &command_line);
    } else {
      CreateProgramFile({kChromeInstallFiles, kLpacChromeInstallFiles},
                        &command_line);
    }

    std::string appcontainer_id =
        testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    appcontainer_id += ".";
    appcontainer_id +=
        testing::UnitTest::GetInstance()->current_test_info()->name();
    TestTargetPolicy policy;
    ResultCode result = SandboxWin::AddAppContainerProfileToPolicy(
        command_line, sandbox_type, appcontainer_id, &policy);
    if (result == SBOX_ALL_OK)
      *profile = policy.GetAppContainerBase();
    return result;
  }

  base::ScopedTempDir temp_dir_;
};

}  // namespace

TEST_F(SandboxWinTest, IsGpuAppContainerEnabled) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, sandbox::mojom::Sandbox::kGpu));
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGpuAppContainer);
  EXPECT_TRUE(SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, sandbox::mojom::Sandbox::kGpu));
  EXPECT_FALSE(SandboxWin::IsAppContainerEnabledForSandbox(
      command_line, sandbox::mojom::Sandbox::kNoSandbox));
}

TEST_F(SandboxWinTest, AppContainerAccessCheckFail) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  scoped_refptr<AppContainerBase> profile;
  ResultCode result = CreateAppContainerProfile(
      command_line, true, sandbox::mojom::Sandbox::kGpu, &profile);
  EXPECT_EQ(SBOX_ERROR_CREATE_APPCONTAINER_ACCESS_CHECK, result);
  EXPECT_EQ(nullptr, profile);
}

TEST_F(SandboxWinTest, AppContainerCheckProfile) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  scoped_refptr<AppContainerBase> profile;
  ResultCode result = CreateAppContainerProfile(
      command_line, false, sandbox::mojom::Sandbox::kGpu, &profile);
  ASSERT_EQ(SBOX_ALL_OK, result);
  ASSERT_NE(nullptr, profile);
  absl::optional<base::win::Sid> package_sid = base::win::Sid::FromSddlString(
      L"S-1-15-2-2402834154-1919024995-1520873375-1190013510-771931769-"
      L"834570634-3212001585");
  ASSERT_TRUE(package_sid);
  EXPECT_EQ(package_sid, profile->GetPackageSid());
  EXPECT_TRUE(profile->GetEnableLowPrivilegeAppContainer());
  CheckCapabilities(profile.get(), {});
}

TEST_F(SandboxWinTest, AppContainerCheckProfileDisableLpac) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGpuLPAC);
  scoped_refptr<AppContainerBase> profile;
  ResultCode result = CreateAppContainerProfile(
      command_line, false, sandbox::mojom::Sandbox::kGpu, &profile);
  ASSERT_EQ(SBOX_ALL_OK, result);
  ASSERT_NE(nullptr, profile);
  EXPECT_FALSE(profile->GetEnableLowPrivilegeAppContainer());
}

TEST_F(SandboxWinTest, AppContainerCheckProfileAddCapabilities) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAddGpuAppContainerCaps,
                                 "  cap1   ,   cap2   ,");
  scoped_refptr<AppContainerBase> profile;
  ResultCode result = CreateAppContainerProfile(
      command_line, false, sandbox::mojom::Sandbox::kGpu, &profile);
  ASSERT_EQ(SBOX_ALL_OK, result);
  ASSERT_NE(nullptr, profile);
  CheckCapabilities(profile.get(), {L"cap1", L"cap2"});
}

// Disabled due to crbug.com/1210614
TEST_F(SandboxWinTest, DISABLED_BlocklistAddOneDllCheckInBrowser) {
  {  // Block loaded module.
    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(L"kernel32.dll", true, &policy);
    EXPECT_EQ(policy.blocklisted_dlls(),
              std::vector<std::wstring>({L"kernel32.dll"}));
  }

  {  // Block module which is not loaded.
    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(L"notloaded.dll", true, &policy);
    EXPECT_TRUE(policy.blocklisted_dlls().empty());
  }

  {  // Block module loaded by short name.
#if defined(ARCH_CPU_X86)
    const std::wstring short_dll_name = L"pe_ima~1.dll";
    const std::wstring full_dll_name = L"pe_image_test_32.dll";
#elif defined(ARCH_CPU_X86_64)
    const std::wstring short_dll_name = L"pe_ima~2.dll";
    const std::wstring full_dll_name = L"pe_image_test_64.dll";
#elif defined(ARCH_CPU_ARM64)
    const std::wstring short_dll_name = L"pe_ima~3.dll";
    const std::wstring full_dll_name = L"pe_image_test_arm64.dll";
#endif

    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_TEST_DATA, &test_data_dir);
    auto dll_path =
        test_data_dir.AppendASCII("pe_image").Append(short_dll_name);

    base::ScopedNativeLibrary library(dll_path);
    EXPECT_TRUE(library.is_valid());

    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(full_dll_name.c_str(), true, &policy);
    EXPECT_EQ(policy.blocklisted_dlls(),
              std::vector<std::wstring>({short_dll_name, full_dll_name}));
  }
}

TEST_F(SandboxWinTest, BlocklistAddOneDllDontCheckInBrowser) {
  {  // Block module with short name.
    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(L"short.dll", false, &policy);
    EXPECT_EQ(policy.blocklisted_dlls(),
              std::vector<std::wstring>({L"short.dll"}));
  }

  {  // Block module with long name.
    TestTargetPolicy policy;
    BlocklistAddOneDllForTesting(L"thelongname.dll", false, &policy);
    EXPECT_EQ(policy.blocklisted_dlls(),
              std::vector<std::wstring>({L"thelongname.dll", L"thelon~1.dll",
                                         L"thelon~2.dll", L"thelon~3.dll"}));
  }
}

// Sandbox can't reach into content to pull the real policies, so these tests
// merely verifies that various parts of the delegate are called correctly and a
// policy can be generated.
class TestSandboxDelegate : public SandboxDelegate {
 public:
  TestSandboxDelegate(sandbox::mojom::Sandbox sandbox_type)
      : sandbox_type_(sandbox_type) {}
  sandbox::mojom::Sandbox GetSandboxType() override { return sandbox_type_; }
  bool DisableDefaultPolicy() override { return false; }
  bool GetAppContainerId(std::string* appcontainer_id) override {
    NOTREACHED();
    return false;
  }

  MOCK_METHOD1(PreSpawnTarget, bool(TargetPolicy* policy));

  void PostSpawnTarget(base::ProcessHandle process) override {}

  bool ShouldUnsandboxedRunInJob() override { return false; }

  bool CetCompatible() override { return true; }

 private:
  sandbox::mojom::Sandbox sandbox_type_;
};

TEST_F(SandboxWinTest, GeneratedPolicyTest) {
  TestSandboxDelegate test_renderer_delegate(
      sandbox::mojom::Sandbox::kRenderer);
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  base::HandlesToInheritVector handles_to_inherit;
  BrokerServices* broker = SandboxFactory::GetBrokerServices();
  scoped_refptr<TargetPolicy> policy = broker->CreatePolicy();
  // PreSpawn should get called, but not modifying the policy for this test.
  EXPECT_CALL(test_renderer_delegate, PreSpawnTarget(_)).WillOnce(Return(true));
  ResultCode result = SandboxWin::GeneratePolicyForSandboxedProcess(
      cmd_line, switches::kRendererProcess, handles_to_inherit,
      &test_renderer_delegate, policy);
  ASSERT_EQ(ResultCode::SBOX_ALL_OK, result);
  // Check some default values come back. No need to check the exact policy in
  // detail, but just that GeneratePolicyForSandboxedProcess generated some kind
  // of valid policy.
  EXPECT_EQ(IntegrityLevel::INTEGRITY_LEVEL_LOW, policy->GetIntegrityLevel());
  EXPECT_EQ(JobLevel::kLockdown, policy->GetJobLevel());
  EXPECT_EQ(TokenLevel::USER_LOCKDOWN, policy->GetLockdownTokenLevel());
}

TEST_F(SandboxWinTest, GeneratedPolicyTestNoSandbox) {
  TestSandboxDelegate test_unsandboxed_delegate(
      sandbox::mojom::Sandbox::kNoSandbox);
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  base::HandlesToInheritVector handles_to_inherit;
  BrokerServices* broker = SandboxFactory::GetBrokerServices();
  scoped_refptr<TargetPolicy> policy = broker->CreatePolicy();
  // Unsandboxed processes never call the delegate prespawn as there is no
  // policy.
  EXPECT_CALL(test_unsandboxed_delegate, PreSpawnTarget(_)).Times(0);

  ResultCode result = SandboxWin::GeneratePolicyForSandboxedProcess(
      cmd_line, switches::kRendererProcess, handles_to_inherit,
      &test_unsandboxed_delegate, policy);
  ASSERT_EQ(ResultCode::SBOX_ERROR_UNSANDBOXED_PROCESS, result);
}

}  // namespace policy
}  // namespace sandbox
