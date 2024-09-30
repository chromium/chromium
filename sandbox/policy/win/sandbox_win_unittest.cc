// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_win.h"

#include <windows.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/lpac_capability.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/broker_services.h"
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

class TestTargetConfig : public TargetConfig {
 public:
  ~TestTargetConfig() override {}
  bool IsConfigured() const override { return false; }
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
  void SetJobMemoryLimit(size_t memory_limit) override {}
  ResultCode AllowFileAccess(FileSemantics semantics,
                             const wchar_t* pattern) override {
    return SBOX_ALL_OK;
  }
  ResultCode AllowExtraDlls(const wchar_t* pattern) override {
    return SBOX_ALL_OK;
  }
  ResultCode SetFakeGdiInit() override { return SBOX_ALL_OK; }
  void AddDllToUnload(const wchar_t* dll_name) override {
    blocklisted_dlls_.push_back(dll_name);
  }
  const std::vector<std::wstring>& blocklisted_dlls() const {
    return blocklisted_dlls_;
  }
  ResultCode SetIntegrityLevel(IntegrityLevel level) override {
    return SBOX_ALL_OK;
  }
  IntegrityLevel GetIntegrityLevel() const override { return IntegrityLevel{}; }
  void SetDelayedIntegrityLevel(IntegrityLevel level) override {}
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
  void AddRestrictingRandomSid() override {}
  void SetLockdownDefaultDacl() override {}
  void AddKernelObjectToClose(HandleToClose handle_info) override {}
  void SetDisconnectCsrss() override {}

  ResultCode AddAppContainerProfile(const wchar_t* package_name) override {
    app_container_ = AppContainerBase::Open(package_name);
    if (!app_container_) {
      return SBOX_ERROR_CREATE_APPCONTAINER;
    }
    return SBOX_ALL_OK;
  }

  AppContainer* GetAppContainer() override { return app_container_.get(); }

  std::unique_ptr<AppContainerBase> TakeAppContainerBase() {
    return std::move(app_container_);
  }

  void SetDesktop(Desktop desktop) override {}
  void SetFilterEnvironment(bool env) override {}
  bool GetEnvironmentFiltered() override { return false; }
  void SetZeroAppShim() override {}

 private:
  std::vector<std::wstring> blocklisted_dlls_;
  std::unique_ptr<AppContainerBase> app_container_;
};

// Drops a temporary file granting RX access to a list of capabilities.
bool DropTempFileWithSecurity(
    const base::ScopedTempDir& temp_dir,
    const std::initializer_list<std::wstring>& capabilities,
    base::FilePath* path) {
  if (!base::CreateTemporaryFileInDir(temp_dir.GetPath(), path)) {
    return false;
  }

  base::win::SecurityDescriptor sd;
  CHECK(sd.SetDaclEntry(base::win::WellKnownSid::kWorld,
                        base::win::SecurityAccessMode::kGrant, GENERIC_ALL, 0));
  for (const std::wstring& capability : capabilities) {
    CHECK(sd.SetDaclEntry(base::win::Sid::FromNamedCapability(capability),
                          base::win::SecurityAccessMode::kGrant,
                          GENERIC_READ | GENERIC_EXECUTE, 0));
  }
  sd.set_dacl_protected(true);
  return sd.WriteToFile(*path, DACL_SECURITY_INFORMATION);
}

void AddSidsToSet(std::set<std::wstring>& sid_set,
                  const std::vector<base::win::Sid>& sids) {
  for (const base::win::Sid& sid : sids) {
    sid_set.insert(*sid.ToSddlString());
  }
}

void AddSidsToSet(std::set<std::wstring>& sid_set,
                  const std::vector<std::wstring>& sids) {
  AddSidsToSet(sid_set, base::win::Sid::FromNamedCapabilityVector(sids));
}

void CompareSidList(const std::set<std::wstring>& sid_set,
                    const std::vector<base::win::Sid>& compare_sids) {
  std::set<std::wstring> compare_set;
  AddSidsToSet(compare_set, compare_sids);
  EXPECT_EQ(sid_set, compare_set);
}

base::FilePath GetShortPathName(const base::FilePath& path) {
  WCHAR short_path[MAX_PATH];
  DWORD size = ::GetShortPathName(path.value().c_str(), short_path,
                                  std::size(short_path));
  if (size == 0 || size >= MAX_PATH) {
    return {};
  }
  return base::FilePath(short_path);
}

struct AppContainerProfileTest {
  sandbox::mojom::Sandbox sandbox_type;
  std::wstring package_sid;
  bool lpac_enabled;
  std::vector<std::wstring> capabilities;
  std::vector<std::wstring> impersonation_capabilities;

  void Check(
      base::expected<std::unique_ptr<AppContainerBase>, ResultCode> result,
      const std::vector<std::wstring>& additional_capabilities) const {
    ASSERT_TRUE(result.has_value());
    auto profile = std::move(result.value());
    ASSERT_NE(nullptr, profile);
    EXPECT_EQ(package_sid, profile->GetPackageSid().ToSddlString());
    EXPECT_EQ(profile->GetEnableLowPrivilegeAppContainer(), lpac_enabled);

    std::set<std::wstring> base_caps;
    AddSidsToSet(base_caps, capabilities);
    AddSidsToSet(base_caps, additional_capabilities);
    std::set<std::wstring> impersonation_caps(base_caps.begin(),
                                              base_caps.end());
    AddSidsToSet(impersonation_caps, impersonation_capabilities);

    CompareSidList(base_caps, profile->GetCapabilities());
    CompareSidList(impersonation_caps, profile->GetImpersonationCapabilities());
  }
};

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

  base::expected<std::unique_ptr<AppContainerBase>, ResultCode>
  CreateAppContainerProfile(const base::CommandLine& base_command_line,
                            bool access_check_fail,
                            sandbox::mojom::Sandbox sandbox_type) {
    base::FilePath path;
    base::CommandLine command_line(base_command_line);

    if (access_check_fail) {
      CreateProgramFile({}, &command_line);
    } else {
      CreateProgramFile({kChromeInstallFiles, kLpacChromeInstallFiles},
                        &command_line);
    }

    std::string appcontainer_id = testing::UnitTest::GetInstance()
                                      ->current_test_info()
                                      ->test_suite_name();
    appcontainer_id += ".";
    appcontainer_id +=
        testing::UnitTest::GetInstance()->current_test_info()->name();
    TestTargetConfig config;
    ResultCode result = SandboxWin::AddAppContainerProfileToConfig(
        command_line, sandbox_type, appcontainer_id, &config);
    if (result != SBOX_ALL_OK) {
      return base::unexpected(result);
    }
    return config.TakeAppContainerBase();
  }

  base::ScopedTempDir temp_dir_;
};

}  // namespace

TEST_F(SandboxWinTest, IsGpuAppContainerEnabled) {
  // Unlike the other tests below that merely test App Container behavior, and
  // can rely on RS1 version check, the GPU App Container feature is gated on
  // RS5. See sandbox::features::IsAppContainerSandboxSupported.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS5) {
    return;
  }
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
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1) {
    return;
  }
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  auto result = CreateAppContainerProfile(command_line, true,
                                          sandbox::mojom::Sandbox::kGpu);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(SBOX_ERROR_CREATE_APPCONTAINER_ACCESS_CHECK, result.error());
}

TEST_F(SandboxWinTest, AppContainerCheckProfile) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1) {
    return;
  }
  constexpr wchar_t kInternetClient[] = L"internetClient";
  constexpr wchar_t kPrivateNetworkClientServer[] =
      L"privateNetworkClientServer";
  constexpr wchar_t kEnterpriseAuthentication[] = L"enterpriseAuthentication";

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  const AppContainerProfileTest kProfileTests[] = {
      {sandbox::mojom::Sandbox::kGpu,
       L"S-1-15-2-2402834154-1919024995-1520873375-1190013510-771931769-"
       L"834570634-3212001585",
       true,
       {kLpacPnpNotifications, kLpacChromeInstallFiles, kRegistryRead},
       {kChromeInstallFiles}},
      {sandbox::mojom::Sandbox::kXrCompositing,
       L"S-1-15-2-1030503276-452227668-393455601-3654269295-1389305662-"
       L"158182952-2716868087",
       false,
       {kLpacPnpNotifications, kLpacChromeInstallFiles, kRegistryRead,
        kChromeInstallFiles},
       {}},
      {sandbox::mojom::Sandbox::kMediaFoundationCdm,
       L"S-1-15-2-3120300879-4058611061-160032764-3562819503-6834604-256341318-"
       L"1442147363",
       true,
       {kInternetClient, kPrivateNetworkClientServer, kLpacChromeInstallFiles,
        kRegistryRead, kLpacCom, kLpacIdentityServices, kLpacMedia,
        kLpacPnPNotifications, kLpacServicesManagement, kLpacSessionManagement,
        kLpacAppExperience, kLpacInstrumentation, kLpacCryptoServices,
        kLpacEnterprisePolicyChangeNotifications, kMediaFoundationCdmFiles,
        kMediaFoundationCdmData},
       {}},
      {sandbox::mojom::Sandbox::kNetwork,
       L"S-1-15-2-1204153576-2881085000-2101973085-273300490-2415804912-"
       L"3587146283-1585457728",
       base::FeatureList::IsEnabled(
           features::kWinSboxNetworkServiceSandboxIsLPAC),
       {kInternetClient, kPrivateNetworkClientServer, kEnterpriseAuthentication,
        kLpacIdentityServices, kLpacCryptoServices, kLpacChromeInstallFiles,
        kRegistryRead},
       {}},
      {sandbox::mojom::Sandbox::kWindowsSystemProxyResolver,
       L"S-1-15-2-1733900417-1595997880-1847635518-1308794714-877418578-"
       L"3685220290-3324296907",
       true,
       {kInternetClient, kLpacServicesManagement,
        kLpacEnterprisePolicyChangeNotifications, kLpacChromeInstallFiles,
        kRegistryRead},
       {}},
  };
  for (const AppContainerProfileTest& test : kProfileTests) {
    test.Check(
        CreateAppContainerProfile(command_line, false, test.sandbox_type), {});
  }
}

TEST_F(SandboxWinTest, AppContainerCheckProfileDisableLpac) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1) {
    return;
  }
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGpuLPAC);
  auto result = CreateAppContainerProfile(command_line, false,
                                          sandbox::mojom::Sandbox::kGpu);
  ASSERT_TRUE(result.has_value());
  ASSERT_NE(nullptr, result.value());
  EXPECT_FALSE(result.value()->GetEnableLowPrivilegeAppContainer());
}

TEST_F(SandboxWinTest, AppContainerCheckProfileAddCapabilities) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1) {
    return;
  }
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAddGpuAppContainerCaps,
                                 "  cap1   ,   cap2   ,");
  const AppContainerProfileTest test{
      sandbox::mojom::Sandbox::kGpu,
      L"S-1-15-2-342359568-3976368142-3454201986-142512210-2527158890-"
      L"3531919343-1556627910",
      true,
      {kLpacPnpNotifications, kLpacChromeInstallFiles, kRegistryRead},
      {kChromeInstallFiles}};
  test.Check(CreateAppContainerProfile(command_line, false,
                                       sandbox::mojom::Sandbox::kGpu),
             {L"cap1", L"cap2"});
}

TEST_F(SandboxWinTest, BlocklistAddOneDllCheckInBrowser) {
  {  // Block loaded module.
    TestTargetConfig config;
    BlocklistAddOneDllForTesting(L"kernel32.dll", &config);
    EXPECT_EQ(config.blocklisted_dlls(),
              std::vector<std::wstring>({L"kernel32.dll"}));
  }

  {  // Block module which is not loaded.
    TestTargetConfig config;
    BlocklistAddOneDllForTesting(L"notloaded.dll", &config);
    EXPECT_TRUE(config.blocklisted_dlls().empty());
  }

  {
    base::FilePath executable_path;
    ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &executable_path));
    constexpr wchar_t kFullDllName[] = L"longfilename.dll";
    base::FilePath dll_path = temp_dir_.GetPath().Append(kFullDllName);

    ASSERT_TRUE(base::CopyFile(executable_path, dll_path));
    base::FilePath short_path = GetShortPathName(dll_path);
    base::FilePath short_name = short_path.BaseName();
    if (short_path.empty() ||
        base::EqualsCaseInsensitiveASCII(short_name.value(), kFullDllName)) {
      LOG(WARNING) << short_path.value()
                   << " doesn't have a short path. Ignoring remaining tests.";
      return;
    }

    base::ScopedNativeLibrary library(short_path);
    ASSERT_TRUE(library.is_valid());

    TestTargetConfig config;
    BlocklistAddOneDllForTesting(kFullDllName, &config);
    EXPECT_EQ(config.blocklisted_dlls(),
              std::vector<std::wstring>({short_name.value(), kFullDllName}));
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
  }

  MOCK_METHOD1(InitializeConfig, bool(TargetConfig* config));
  MOCK_METHOD1(PreSpawnTarget, bool(TargetPolicy* policy));

  std::string GetSandboxTag() override { return std::string(); }
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
  auto policy = broker->CreatePolicy();
  EXPECT_CALL(test_renderer_delegate, InitializeConfig(_))
      .WillOnce(Return(true));
  // PreSpawn should get called, but not modifying the policy for this test.
  EXPECT_CALL(test_renderer_delegate, PreSpawnTarget(_)).WillOnce(Return(true));
  ResultCode result = SandboxWin::GeneratePolicyForSandboxedProcess(
      cmd_line, handles_to_inherit, &test_renderer_delegate, policy.get());
  ASSERT_EQ(ResultCode::SBOX_ALL_OK, result);
  // Check some default values come back. No need to check the exact policy in
  // detail, but just that GeneratePolicyForSandboxedProcess generated some kind
  // of valid policy.
  EXPECT_EQ(IntegrityLevel::INTEGRITY_LEVEL_LOW,
            policy->GetConfig()->GetIntegrityLevel());
  EXPECT_EQ(JobLevel::kLockdown, policy->GetConfig()->GetJobLevel());
  EXPECT_EQ(TokenLevel::USER_LOCKDOWN,
            policy->GetConfig()->GetLockdownTokenLevel());
}

TEST_F(SandboxWinTest, GeneratedPolicyTestMultipleCalls) {
  TestSandboxDelegate test_renderer_delegate(
      sandbox::mojom::Sandbox::kRenderer);
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  base::HandlesToInheritVector handles_to_inherit;
  BrokerServices* broker = SandboxFactory::GetBrokerServices();
  auto policy = broker->CreatePolicy();

  // Checks that multiple initializations of the policy only initialize the
  // configuration once but calls PreSpawnTarget twice.
  EXPECT_CALL(test_renderer_delegate, InitializeConfig(_))
      .WillOnce(Return(true));
  EXPECT_CALL(test_renderer_delegate, PreSpawnTarget(_))
      .Times(2)
      .WillRepeatedly(Return(true));
  ResultCode result = SandboxWin::GeneratePolicyForSandboxedProcess(
      cmd_line, handles_to_inherit, &test_renderer_delegate, policy.get());
  ASSERT_EQ(ResultCode::SBOX_ALL_OK, result);
  BrokerServicesBase::FreezeTargetConfigForTesting(policy->GetConfig());
  result = SandboxWin::GeneratePolicyForSandboxedProcess(
      cmd_line, handles_to_inherit, &test_renderer_delegate, policy.get());
  ASSERT_EQ(ResultCode::SBOX_ALL_OK, result);
}

TEST_F(SandboxWinTest, GeneratedPolicyTestNoSandbox) {
  TestSandboxDelegate test_unsandboxed_delegate(
      sandbox::mojom::Sandbox::kNoSandbox);
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  base::HandlesToInheritVector handles_to_inherit;
  BrokerServices* broker = SandboxFactory::GetBrokerServices();
  auto policy = broker->CreatePolicy();
  // Unsandboxed processes never call the delegate prespawn as there is no
  // policy.
  EXPECT_CALL(test_unsandboxed_delegate, InitializeConfig(_)).Times(0);
  EXPECT_CALL(test_unsandboxed_delegate, PreSpawnTarget(_)).Times(0);

  ResultCode result = SandboxWin::GeneratePolicyForSandboxedProcess(
      cmd_line, handles_to_inherit, &test_unsandboxed_delegate, policy.get());
  ASSERT_EQ(ResultCode::SBOX_ERROR_UNSANDBOXED_PROCESS, result);
}

TEST_F(SandboxWinTest, GetJobMemoryLimit) {
  constexpr uint64_t k8GB = 8192;
#if defined(ARCH_CPU_64_BITS)
  constexpr uint64_t kGB = 1024 * 1024 * 1024;
  constexpr uint64_t k65GB = 66560;
  constexpr uint64_t k33GB = 33792;
  constexpr uint64_t k17GB = 17408;

  // Test GPU with physical memory > 64GB.
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(k65GB);
    std::optional<size_t> memory_limit =
        SandboxWin::GetJobMemoryLimit(sandbox::mojom::Sandbox::kGpu);
    EXPECT_TRUE(memory_limit.has_value());
    EXPECT_EQ(memory_limit, 64 * kGB);
  }

  // Test GPU with physical memory > 32GB
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(k33GB);
    std::optional<size_t> memory_limit =
        SandboxWin::GetJobMemoryLimit(sandbox::mojom::Sandbox::kGpu);
    EXPECT_TRUE(memory_limit.has_value());
    EXPECT_EQ(memory_limit, 32 * kGB);
  }

  // Test GPU with physical memory > 16GB
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(k17GB);
    std::optional<size_t> memory_limit =
        SandboxWin::GetJobMemoryLimit(sandbox::mojom::Sandbox::kGpu);
    EXPECT_TRUE(memory_limit.has_value());
    EXPECT_EQ(memory_limit, 16 * kGB);
  }

  // Test GPU with physical memory < 16GB
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(k8GB);
    std::optional<size_t> memory_limit =
        SandboxWin::GetJobMemoryLimit(sandbox::mojom::Sandbox::kGpu);
    EXPECT_TRUE(memory_limit.has_value());
    EXPECT_EQ(memory_limit, 8 * kGB);
  }

  // Test that Renderer has high (1TB) memory limit.
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(k8GB);
    std::optional<size_t> memory_limit =
        SandboxWin::GetJobMemoryLimit(sandbox::mojom::Sandbox::kRenderer);
    EXPECT_TRUE(memory_limit.has_value());
    EXPECT_EQ(memory_limit, 1024 * kGB);
  }
#else
  // Test 32-bit processes don't get a limit.
  {
    base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(k8GB);
    std::optional<size_t> memory_limit =
        SandboxWin::GetJobMemoryLimit(sandbox::mojom::Sandbox::kRenderer);
    EXPECT_FALSE(memory_limit.has_value());
  }
#endif  // defined(ARCH_CPU_64_BITS)
}

}  // namespace policy
}  // namespace sandbox
