// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/process/process_info.h"
#include "base/rand_util.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "sandbox/features.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/process_mitigations_unittest.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

const wchar_t kAppContainerSid[] =
    L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
    L"924012148-2839372144";

std::wstring GenerateRandomPackageName() {
  return base::ASCIIToWide(base::StringPrintf(
      "%016" PRIX64 "%016" PRIX64, base::RandUint64(), base::RandUint64()));
}

const char* TokenTypeToName(bool impersonation) {
  return impersonation ? "Impersonation Token" : "Primary Token";
}

void CheckToken(const std::optional<base::win::AccessToken>& token,
                bool impersonation,
                PSECURITY_CAPABILITIES security_capabilities,
                bool restricted) {
  ASSERT_TRUE(token);
  EXPECT_EQ(restricted, token->IsRestricted())
      << TokenTypeToName(impersonation);
  EXPECT_TRUE(token->IsAppContainer()) << TokenTypeToName(impersonation);
  EXPECT_EQ(token->IsImpersonation(), impersonation)
      << TokenTypeToName(impersonation);
  if (impersonation) {
    EXPECT_FALSE(token->IsIdentification()) << TokenTypeToName(impersonation);
  }

  std::optional<base::win::Sid> package_sid = token->AppContainerSid();
  ASSERT_TRUE(package_sid) << TokenTypeToName(impersonation);
  EXPECT_TRUE(package_sid->Equal(security_capabilities->AppContainerSid))
      << TokenTypeToName(impersonation);

  std::vector<base::win::AccessToken::Group> capabilities =
      token->Capabilities();
  ASSERT_EQ(capabilities.size(), security_capabilities->CapabilityCount)
      << TokenTypeToName(impersonation);
  for (size_t index = 0; index < capabilities.size(); ++index) {
    EXPECT_EQ(
        capabilities[index].GetAttributes(),
        UNSAFE_TODO(security_capabilities->Capabilities[index]).Attributes)
        << TokenTypeToName(impersonation);
    EXPECT_TRUE(capabilities[index].GetSid().Equal(
        UNSAFE_TODO(security_capabilities->Capabilities[index]).Sid))
        << TokenTypeToName(impersonation);
  }
}

void CheckProcessToken(HANDLE process,
                       PSECURITY_CAPABILITIES security_capabilities,
                       bool restricted) {
  CheckToken(base::win::AccessToken::FromProcess(process), false,
             security_capabilities, restricted);
}

void CheckThreadToken(HANDLE thread,
                      PSECURITY_CAPABILITIES security_capabilities,
                      bool restricted) {
  CheckToken(base::win::AccessToken::FromThread(thread), true,
             security_capabilities, restricted);
}

// Check for LPAC using an access check. We could query for a security attribute
// but that's undocumented and has the potential to change.
void CheckLpacToken(HANDLE process) {
  auto token =
      base::win::AccessToken::FromProcess(process, /*impersonation=*/true);
  ASSERT_TRUE(token);
  constexpr ACCESS_MASK kACAccess = 0x1;
  constexpr ACCESS_MASK kLPACAccess = 0x2;
  base::win::SecurityDescriptor sd;
  sd.set_owner(base::win::Sid(base::win::WellKnownSid::kLocalSystem));
  sd.set_group(base::win::Sid(base::win::WellKnownSid::kLocalSystem));
  ASSERT_TRUE(sd.SetDaclEntry(base::win::WellKnownSid::kWorld,
                              base::win::SecurityAccessMode::kGrant,
                              kACAccess | kLPACAccess, 0));
  ASSERT_TRUE(sd.SetDaclEntry(base::win::WellKnownSid::kAllApplicationPackages,
                              base::win::SecurityAccessMode::kGrant, kACAccess,
                              0));
  ASSERT_TRUE(sd.SetDaclEntry(
      base::win::WellKnownSid::kAllRestrictedApplicationPackages,
      base::win::SecurityAccessMode::kGrant, kLPACAccess, 0));
  GENERIC_MAPPING generic_mapping = {};
  auto result = sd.AccessCheck(*token, MAXIMUM_ALLOWED, generic_mapping);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->access_status);
  ASSERT_EQ(kLPACAccess, result->granted_access);
}

// Generate a unique sandbox AC profile for the appcontainer based on the SHA1
// hash of the appcontainer_id. This does not need to be secure so using SHA1
// isn't a security concern.
std::wstring GetAppContainerProfileName() {
  std::string sandbox_base_name = std::string("cr.sb.net");
  // Create a unique app container ID for the test case and process. This
  // ensures that if multiple tests (or repeated runs of the same test across
  // parallel launcher jobs) are running concurrently they don't mess with each
  // other's app containers.
  const auto* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  std::string appcontainer_id =
      base::StrCat({test_info->test_suite_name(), test_info->name(),
                    base::NumberToString(::GetCurrentProcessId())});
  auto sha1 = base::SHA1HashString(appcontainer_id);
  std::string profile_name =
      base::StrCat({sandbox_base_name, base::HexEncode(sha1)});
  // CreateAppContainerProfile requires that the profile name is at most 64
  // characters but 50 on WCOS systems.  The size of sha1 is a constant 40, so
  // validate that the base names are sufficiently short that the total length
  // is valid on all systems.
  DCHECK_LE(profile_name.length(), 50U);
  return base::UTF8ToWide(profile_name);
}

// Adds an app container policy similar to network service.
ResultCode AddNetworkAppContainerPolicy(TargetPolicy* policy) {
  std::wstring profile_name = GetAppContainerProfileName();
  ResultCode ret = policy->GetConfig()->AddAppContainerProfile(profile_name);
  if (SBOX_ALL_OK != ret)
    return ret;
  ret = policy->GetConfig()->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  if (SBOX_ALL_OK != ret)
    return ret;
  AppContainer* app_container = policy->GetConfig()->GetAppContainer();

  constexpr const base::wcstring_view kBaseCapsSt[] = {
      L"lpacChromeInstallFiles", L"registryRead", L"lpacIdentityServices",
      L"lpacCryptoServices"};
  constexpr const base::win::WellKnownCapability kBaseCapsWK[] = {
      base::win::WellKnownCapability::kPrivateNetworkClientServer,
      base::win::WellKnownCapability::kInternetClient,
      base::win::WellKnownCapability::kEnterpriseAuthentication};

  for (const auto& cap : kBaseCapsSt) {
    app_container->AddCapability(cap);
  }

  for (const auto cap : kBaseCapsWK) {
    app_container->AddCapability(cap);
  }

  app_container->SetEnableLowPrivilegeAppContainer(true);

  return SBOX_ALL_OK;
}

class AppContainerTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (!features::IsAppContainerSandboxSupported())
      return;
    package_name_ = GenerateRandomPackageName();
    broker_services_ = GetBroker();
    policy_ = broker_services_->CreatePolicy();
    ASSERT_EQ(SBOX_ALL_OK, policy_->GetConfig()->SetProcessMitigations(
                               MITIGATION_HEAP_TERMINATE));
    ASSERT_EQ(SBOX_ALL_OK,
              policy_->GetConfig()->AddAppContainerProfile(package_name_));
    created_profile_ = true;
  }

  void TearDown() override {
    if (scoped_process_info_.IsValid()) {
      ::TerminateProcess(scoped_process_info_.process_handle(), 0);
    }
    if (created_profile_) {
      AppContainerBase::Delete(package_name_);
    }
  }

 protected:
  void CreateProcess() {
    // Get the path to the sandboxed app.
    base::FilePath prog_name;
    ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &prog_name));
    base::CommandLine cmd_line(prog_name);

    DWORD last_error = 0;
    ResultCode result;
    base::test::TaskEnvironment task_environment;
    base::test::TestFuture<base::win::ScopedProcessInformation, DWORD,
                           ResultCode>
        test_future;
    broker_services_->SpawnTargetAsync(cmd_line, std::move(policy_),
                                       test_future.GetCallback());
    std::tie(scoped_process_info_, last_error, result) = test_future.Take();
    ASSERT_EQ(SBOX_ALL_OK, result) << "Last Error: " << last_error;
  }

  AppContainerBase* container() {
    // For testing purposes we known the base class so cast directly.
    return static_cast<AppContainerBase*>(
        policy_->GetConfig()->GetAppContainer());
  }

  std::wstring package_name_;
  bool created_profile_ = false;
  raw_ptr<BrokerServices> broker_services_;
  std::unique_ptr<TargetPolicy> policy_;
  base::win::ScopedProcessInformation scoped_process_info_;
};

}  // namespace

SBOX_TEST_COMMAND(AppContainerEventOpenCommand) {
  if (args.empty()) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  base::win::ScopedHandle event_open(
      ::OpenEvent(EVENT_ALL_ACCESS, false, args[0].c_str()));
  DWORD error_open = ::GetLastError();

  if (event_open.is_valid()) {
    return SBOX_TEST_SUCCEEDED;
  }

  if (ERROR_ACCESS_DENIED == error_open)
    return SBOX_TEST_DENIED;

  return SBOX_TEST_FAILED;
}

TEST(LowBoxTest, DenyOpenEventForLowBox) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  base::win::ScopedHandle event(
      ::CreateEvent(nullptr, false, false, kAppContainerSid));
  ASSERT_TRUE(event.is_valid());

  AppContainerEventOpenCommandTestRunner runner(
      JobLevel::kUnprotected, USER_UNPROTECTED, USER_UNPROTECTED);
  EXPECT_EQ(SBOX_ALL_OK, runner.GetConfig()->SetLowBox(kAppContainerSid));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(kAppContainerSid));
}

TEST_F(AppContainerTest, CheckIncompatibleOptions) {
  if (!created_profile_) {
    return;
  }

  EXPECT_EQ(SBOX_ERROR_BAD_PARAMS,
            policy_->GetConfig()->SetIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED));
  EXPECT_EQ(SBOX_ERROR_BAD_PARAMS,
            policy_->GetConfig()->SetLowBox(kAppContainerSid));

  MitigationFlags expected_mitigations = 0;
  MitigationFlags expected_delayed = MITIGATION_HEAP_TERMINATE;
  sandbox::ResultCode expected_result = SBOX_ERROR_BAD_PARAMS;

  if (base::win::GetVersion() >= base::win::Version::WIN10_RS5) {
    expected_mitigations = MITIGATION_HEAP_TERMINATE;
    expected_delayed = 0;
    expected_result = SBOX_ALL_OK;
  }

  EXPECT_EQ(expected_mitigations,
            policy_->GetConfig()->GetProcessMitigations());
  EXPECT_EQ(expected_delayed,
            policy_->GetConfig()->GetDelayedProcessMitigations());
  EXPECT_EQ(expected_result, policy_->GetConfig()->SetProcessMitigations(
                                 MITIGATION_HEAP_TERMINATE));
}

TEST_F(AppContainerTest, NoCapabilities) {
  if (!created_profile_) {
    return;
  }

  EXPECT_EQ(SBOX_ALL_OK, policy_->GetConfig()->SetTokenLevel(USER_UNPROTECTED,
                                                             USER_UNPROTECTED));
  EXPECT_EQ(SBOX_ALL_OK,
            policy_->GetConfig()->SetJobLevel(JobLevel::kUnprotected, 0));

  auto security_capabilities = container()->GetSecurityCapabilities();
  CreateProcess();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), FALSE);
}

TEST_F(AppContainerTest, NoCapabilitiesRestricted) {
  if (!created_profile_) {
    return;
  }

  EXPECT_EQ(SBOX_ALL_OK, policy_->GetConfig()->SetTokenLevel(
                             USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN));
  EXPECT_EQ(SBOX_ALL_OK,
            policy_->GetConfig()->SetJobLevel(JobLevel::kUnprotected, 0));

  auto security_capabilities = container()->GetSecurityCapabilities();
  CreateProcess();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), TRUE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), TRUE);
}

TEST_F(AppContainerTest, WithCapabilities) {
  if (!created_profile_) {
    return;
  }

  container()->AddCapability(base::win::WellKnownCapability::kInternetClient);
  container()->AddCapability(
      base::win::WellKnownCapability::kInternetClientServer);
  EXPECT_EQ(SBOX_ALL_OK, policy_->GetConfig()->SetTokenLevel(USER_UNPROTECTED,
                                                             USER_UNPROTECTED));
  EXPECT_EQ(SBOX_ALL_OK,
            policy_->GetConfig()->SetJobLevel(JobLevel::kUnprotected, 0));

  auto security_capabilities = container()->GetSecurityCapabilities();
  CreateProcess();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), FALSE);
}

TEST_F(AppContainerTest, WithCapabilitiesRestricted) {
  if (!created_profile_) {
    return;
  }

  container()->AddCapability(base::win::WellKnownCapability::kInternetClient);
  container()->AddCapability(
      base::win::WellKnownCapability::kInternetClientServer);
  EXPECT_EQ(SBOX_ALL_OK, policy_->GetConfig()->SetTokenLevel(
                             USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN));
  EXPECT_EQ(SBOX_ALL_OK,
            policy_->GetConfig()->SetJobLevel(JobLevel::kUnprotected, 0));

  auto security_capabilities = container()->GetSecurityCapabilities();
  CreateProcess();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), TRUE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), TRUE);
}

TEST_F(AppContainerTest, WithImpersonationCapabilities) {
  if (!created_profile_) {
    return;
  }

  container()->AddCapability(base::win::WellKnownCapability::kInternetClient);
  container()->AddCapability(
      base::win::WellKnownCapability::kInternetClientServer);
  container()->AddImpersonationCapability(
      base::win::WellKnownCapability::kPrivateNetworkClientServer);
  container()->AddImpersonationCapability(
      base::win::WellKnownCapability::kPicturesLibrary);
  EXPECT_EQ(SBOX_ALL_OK, policy_->GetConfig()->SetTokenLevel(USER_UNPROTECTED,
                                                             USER_UNPROTECTED));
  EXPECT_EQ(SBOX_ALL_OK,
            policy_->GetConfig()->SetJobLevel(JobLevel::kUnprotected, 0));

  auto security_capabilities = container()->GetSecurityCapabilities();
  SecurityCapabilities impersonation_security_capabilities(
      container()->GetPackageSid(),
      container()->GetImpersonationCapabilities());

  CreateProcess();
  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);

  CheckThreadToken(scoped_process_info_.thread_handle(),
                   &impersonation_security_capabilities, FALSE);
}

TEST_F(AppContainerTest, NoCapabilitiesLPAC) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  container()->SetEnableLowPrivilegeAppContainer(true);
  EXPECT_EQ(SBOX_ALL_OK, policy_->GetConfig()->SetTokenLevel(USER_UNPROTECTED,
                                                             USER_UNPROTECTED));
  EXPECT_EQ(SBOX_ALL_OK,
            policy_->GetConfig()->SetJobLevel(JobLevel::kUnprotected, 0));

  auto security_capabilities = container()->GetSecurityCapabilities();
  CreateProcess();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), FALSE);
  CheckLpacToken(scoped_process_info_.process_handle());
}

SBOX_TEST_COMMAND(LoadDLL) {
  // Library here doesn't matter as long as it's in the output directory: re-use
  // one from another sbox test.
  base::ScopedNativeLibrary test_dll(
      base::FilePath(FILE_PATH_LITERAL("sbox_integration_test_win_proc.exe")));
  if (test_dll.is_valid()) {
    return SBOX_TEST_SUCCEEDED;
  }
  return SBOX_TEST_FAILED;
}

SBOX_TEST_COMMAND(CheckIsAppContainer) {
  if (base::IsCurrentProcessInAppContainer()) {
    return SBOX_TEST_SUCCEEDED;
  }
  return SBOX_TEST_FAILED;
}

TEST(AppContainerLaunchTest, CheckLPACACE) {
  if (!features::IsAppContainerSandboxSupported())
    return;
  LoadDLLTestRunner runner;
  AddNetworkAppContainerPolicy(runner.GetPolicy());

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());

  AppContainerBase::Delete(GetAppContainerProfileName());
}

TEST(AppContainerLaunchTest, IsAppContainer) {
  if (!features::IsAppContainerSandboxSupported())
    return;
  CheckIsAppContainerTestRunner runner;
  AddNetworkAppContainerPolicy(runner.GetPolicy());

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());

  AppContainerBase::Delete(GetAppContainerProfileName());
}

TEST(AppContainerLaunchTest, IsNotAppContainer) {
  CheckIsAppContainerTestRunner runner;

  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest());
}

SBOX_TEST_COMMAND(CreateTempFileInAppContainer) {
  if (!base::IsCurrentProcessInAppContainer()) {
    return SBOX_TEST_FIRST_ERROR;
  }
  base::FilePath temp_file;
  if (!base::CreateTemporaryFile(&temp_file)) {
    return SBOX_TEST_SECOND_ERROR;
  }
  return SBOX_TEST_SUCCEEDED;
}

TEST(AppContainerLaunchTest, CreateTempFile) {
  if (!features::IsAppContainerSandboxSupported()) {
    return;
  }
  CreateTempFileInAppContainerTestRunner runner;
  std::wstring package_name = GenerateRandomPackageName();
  ASSERT_EQ(SBOX_ALL_OK,
            runner.GetConfig()->AddAppContainerProfile(package_name));
  EXPECT_EQ(SBOX_ALL_OK, runner.GetConfig()->SetTokenLevel(USER_UNPROTECTED,
                                                           USER_UNPROTECTED));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest());
  EXPECT_TRUE(AppContainerBase::Delete(package_name));
}

TEST(LowBoxTest, ChildProcessMitigationLowBox) {
  if (!features::IsAppContainerSandboxSupported()) {
    return;
  }

  TestChildProcessTestRunner runner(JobLevel::kLimitedUser, USER_UNPROTECTED,
                                    USER_UNPROTECTED);

  EXPECT_EQ(SBOX_ALL_OK, runner.GetConfig()->SetLowBox(kAppContainerSid));

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  EXPECT_EQ(SBOX_TEST_SECOND_ERROR, runner.RunTest(cmd.value(), L"false"));
}

// Component build DLLs statically link user32.dll and gdi32.dll, so delay-load
// behavior can only be tested in non-component builds.
#if !defined(COMPONENT_BUILD)
SBOX_TEST_COMMAND(CheckIsUser32AndGdi32Available) {
  if (args.size() != 1) {
    return SBOX_TEST_INVALID_PARAMETER;
  }
  const bool expect_available = (args[0] == L"true");

  // Win32k syscalls are enabled in this test so IsUser32AndGdi32Available()
  // exercises the delay-load path rather than the mitigation policy check.
  PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
  if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                    ProcessSystemCallDisablePolicy, &policy,
                                    sizeof(policy)) ||
      policy.DisallowWin32kSystemCalls != 0) {
    return SBOX_TEST_FIRST_ERROR;
  }

  // user32.dll and gdi32.dll are delay-loaded and must not be loaded yet.
  if (::GetModuleHandleW(L"user32.dll") != nullptr ||
      ::GetModuleHandleW(L"gdi32.dll") != nullptr) {
    return SBOX_TEST_FIRST_ERROR;
  }

  // Force-enable pinning so this test exercises the user32.dll/gdi32.dll load
  // path regardless of channel or trial group.
  base::win::EnablePinUser32AndGdi32ForTesting();

  if (base::win::IsUser32AndGdi32Available() != expect_available) {
    return SBOX_TEST_SECOND_ERROR;
  }

  if (expect_available) {
    if (::GetModuleHandleW(L"user32.dll") == nullptr ||
        ::GetModuleHandleW(L"gdi32.dll") == nullptr) {
      return SBOX_TEST_THIRD_ERROR;
    }
  } else {
    if (::GetModuleHandleW(L"user32.dll") != nullptr ||
        base::win::PinUser32()) {
      return SBOX_TEST_FOURTH_ERROR;
    }
  }

  // Callers guarded by IsUser32AndGdi32Available() must not crash in the
  // delay-load failure hook when user32.dll fails DllMain initialization.
  base::win::EnableHighDPISupport();
  return SBOX_TEST_SUCCEEDED;
}

TEST(AppContainerLaunchTest, IsUser32AndGdi32AvailableDefaultDesktop) {
  if (!features::IsAppContainerSandboxSupported()) {
    return;
  }
  CheckIsUser32AndGdi32AvailableTestRunner runner;
  ASSERT_EQ(SBOX_ALL_OK, AddNetworkAppContainerPolicy(runner.GetPolicy()));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"true"));

  AppContainerBase::Delete(GetAppContainerProfileName());
}

namespace {

class CreateAlternateDesktopDelegate : public base::PlatformThread::Delegate {
 public:
  CreateAlternateDesktopDelegate(BrokerServices* broker, HDESK template_desktop)
      : broker_(broker), template_desktop_(template_desktop) {}
  CreateAlternateDesktopDelegate(const CreateAlternateDesktopDelegate&) =
      delete;
  CreateAlternateDesktopDelegate& operator=(
      const CreateAlternateDesktopDelegate&) = delete;

  void ThreadMain() override {
    HDESK orig_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
    set_desktop_ok_ = ::SetThreadDesktop(template_desktop_);
    if (set_desktop_ok_) {
      result_ = broker_->CreateAlternateDesktop(Desktop::kAlternateDesktop);
      restore_desktop_ok_ = ::SetThreadDesktop(orig_desktop);
    }
  }

  BOOL set_desktop_ok() const { return set_desktop_ok_; }
  BOOL restore_desktop_ok() const { return restore_desktop_ok_; }
  ResultCode result() const { return result_; }

 private:
  const raw_ptr<BrokerServices> broker_;
  const HDESK template_desktop_;
  BOOL set_desktop_ok_ = FALSE;
  BOOL restore_desktop_ok_ = FALSE;
  ResultCode result_ = SBOX_ERROR_GENERIC;
};

}  // namespace

TEST(AppContainerLaunchTest, IsUser32AndGdi32AvailableNoLpacDesktopAccess) {
  if (!features::IsAppContainerSandboxSupported()) {
    return;
  }
  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker);
  broker->DestroyDesktops();
  base::ScopedClosureRunner destroy_desktops(base::BindOnce(
      &BrokerServices::DestroyDesktops, base::Unretained(broker)));

  // Create a desktop whose DACL grants access to Everyone and ALL APPLICATION
  // PACKAGES, but omits ALL RESTRICTED APPLICATION PACKAGES (LPAC).
  base::win::SecurityDescriptor sd;
  ASSERT_TRUE(sd.SetDaclEntry(base::win::WellKnownSid::kWorld,
                              base::win::SecurityAccessMode::kGrant,
                              GENERIC_ALL, 0));
  ASSERT_TRUE(sd.SetDaclEntry(base::win::WellKnownSid::kAllApplicationPackages,
                              base::win::SecurityAccessMode::kGrant,
                              GENERIC_ALL, 0));
  SECURITY_DESCRIPTOR security_desc = sd.ToAbsolute();
  SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), &security_desc,
                                    FALSE};
  std::wstring template_desktop_name =
      base::StrCat({L"lpac_no_access_desktop_",
                    base::NumberToWString(::GetCurrentProcessId())});
  HDESK template_desktop =
      ::CreateDesktop(template_desktop_name.c_str(), nullptr, nullptr, 0,
                      GENERIC_ALL, &attributes);
  ASSERT_NE(template_desktop, nullptr);
  base::ScopedClosureRunner close_template_desktop(base::BindOnce(
      [](HDESK desktop) { ::CloseDesktop(desktop); }, template_desktop));

  // Switch to `template_desktop` on a fresh thread (which cannot have existing
  // windows or hooks that would cause SetThreadDesktop to fail with ERROR_BUSY)
  // so CreateAlternateDesktop copies its DACL.
  CreateAlternateDesktopDelegate create_desktop_delegate(broker,
                                                         template_desktop);
  base::PlatformThreadHandle thread_handle;
  ASSERT_TRUE(base::PlatformThread::Create(0, &create_desktop_delegate,
                                           &thread_handle));
  base::PlatformThread::Join(thread_handle);
  close_template_desktop.RunAndReset();
  ASSERT_TRUE(create_desktop_delegate.set_desktop_ok());
  ASSERT_TRUE(create_desktop_delegate.restore_desktop_ok());
  ASSERT_EQ(SBOX_ALL_OK, create_desktop_delegate.result());

  // Lower the alternate desktop's mandatory label to Low IL so AppContainer
  // tokens are not blocked by mandatory integrity control.
  std::wstring alt_desktop_name =
      broker->GetDesktopName(Desktop::kAlternateDesktop);
  HDESK alt_desktop =
      ::OpenDesktop(alt_desktop_name.c_str(), 0, FALSE, WRITE_OWNER);
  ASSERT_NE(alt_desktop, nullptr);
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            SetObjectIntegrityLabel(alt_desktop,
                                    base::win::SecurityObjectType::kDesktop, 0,
                                    INTEGRITY_LEVEL_LOW));
  ASSERT_TRUE(::CloseDesktop(alt_desktop));

  base::ScopedClosureRunner delete_profile(base::BindOnce(
      [](const std::wstring& name) { AppContainerBase::Delete(name); },
      GetAppContainerProfileName()));

  // A regular AppContainer has ALL APPLICATION PACKAGES and can initialize
  // user32.dll on this desktop.
  {
    CheckIsUser32AndGdi32AvailableTestRunner ac_runner;
    ASSERT_EQ(SBOX_ALL_OK, AddNetworkAppContainerPolicy(ac_runner.GetPolicy()));
    ac_runner.GetConfig()->GetAppContainer()->SetEnableLowPrivilegeAppContainer(
        false);
    ac_runner.GetConfig()->SetDesktop(Desktop::kAlternateDesktop);
    EXPECT_EQ(SBOX_TEST_SUCCEEDED, ac_runner.RunTest(L"true"));
  }

  // An LPAC process lacks ALL RESTRICTED APPLICATION PACKAGES on this desktop,
  // so user32.dll fails DllMain initialization and IsUser32AndGdi32Available()
  // must return false without crashing.
  {
    CheckIsUser32AndGdi32AvailableTestRunner lpac_runner;
    ASSERT_EQ(SBOX_ALL_OK,
              AddNetworkAppContainerPolicy(lpac_runner.GetPolicy()));
    lpac_runner.GetConfig()->SetDesktop(Desktop::kAlternateDesktop);
    EXPECT_EQ(SBOX_TEST_SUCCEEDED, lpac_runner.RunTest(L"false"));
  }
}
#endif  // !defined(COMPONENT_BUILD)

}  // namespace sandbox
