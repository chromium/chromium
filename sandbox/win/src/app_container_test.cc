// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/process_info.h"
#include "base/rand_util.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/security_descriptor.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "sandbox/features.h"
#include "sandbox/win/src/app_container_base.h"
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
    EXPECT_EQ(capabilities[index].GetAttributes(),
              security_capabilities->Capabilities[index].Attributes)
        << TokenTypeToName(impersonation);
    EXPECT_TRUE(capabilities[index].GetSid().Equal(
        security_capabilities->Capabilities[index].Sid))
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
  // Create a unique app container ID for the test case. This ensures that if
  // multiple tests are running concurrently they don't mess with each other's
  // app containers.
  std::string appcontainer_id(
      testing::UnitTest::GetInstance()->current_test_info()->test_suite_name());
  appcontainer_id +=
      testing::UnitTest::GetInstance()->current_test_info()->name();
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
  ResultCode ret =
      policy->GetConfig()->AddAppContainerProfile(profile_name.c_str());
  if (SBOX_ALL_OK != ret)
    return ret;
  ret = policy->GetConfig()->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  if (SBOX_ALL_OK != ret)
    return ret;
  AppContainer* app_container = policy->GetConfig()->GetAppContainer();

  constexpr const wchar_t* kBaseCapsSt[] = {
      L"lpacChromeInstallFiles", L"registryRead", L"lpacIdentityServices",
      L"lpacCryptoServices"};
  constexpr const base::win::WellKnownCapability kBaseCapsWK[] = {
      base::win::WellKnownCapability::kPrivateNetworkClientServer,
      base::win::WellKnownCapability::kInternetClient,
      base::win::WellKnownCapability::kEnterpriseAuthentication};

  for (const auto* cap : kBaseCapsSt) {
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
    ASSERT_EQ(SBOX_ALL_OK, policy_->GetConfig()->AddAppContainerProfile(
                               package_name_.c_str()));
    created_profile_ = true;
  }

  void TearDown() override {
    if (scoped_process_info_.IsValid()) {
      ::TerminateProcess(scoped_process_info_.process_handle(), 0);
    }
    if (created_profile_) {
      AppContainerBase::Delete(package_name_.c_str());
    }
  }

 protected:
  void CreateProcess() {
    // Get the path to the sandboxed app.
    wchar_t prog_name[MAX_PATH] = {};
    ASSERT_NE(DWORD{0}, ::GetModuleFileNameW(nullptr, prog_name, MAX_PATH));

    PROCESS_INFORMATION process_info = {};
    DWORD last_error = 0;
    ResultCode result = broker_services_->SpawnTarget(
        prog_name, prog_name, std::move(policy_), &last_error, &process_info);
    ASSERT_EQ(SBOX_ALL_OK, result) << "Last Error: " << last_error;
    scoped_process_info_.Set(process_info);
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

SBOX_TESTS_COMMAND int AppContainerEvent_Open(int argc, wchar_t** argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  base::win::ScopedHandle event_open(
      ::OpenEvent(EVENT_ALL_ACCESS, false, argv[0]));
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

  TestRunner runner(JobLevel::kUnprotected, USER_UNPROTECTED, USER_UNPROTECTED);
  EXPECT_EQ(SBOX_ALL_OK,
            runner.GetPolicy()->GetConfig()->SetLowBox(kAppContainerSid));
  std::wstring test_str = L"AppContainerEvent_Open ";
  test_str += kAppContainerSid;
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(test_str.c_str()));
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

SBOX_TESTS_COMMAND int LoadDLL(int argc, wchar_t** argv) {
  // Library here doesn't matter as long as it's in the output directory: re-use
  // one from another sbox test.
  base::ScopedNativeLibrary test_dll(
      base::FilePath(FILE_PATH_LITERAL("sbox_integration_test_win_proc.exe")));
  if (test_dll.is_valid())
    return SBOX_TEST_SUCCEEDED;
  return SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int CheckIsAppContainer(int argc, wchar_t** argv) {
  if (base::IsCurrentProcessInAppContainer())
    return SBOX_TEST_SUCCEEDED;
  return SBOX_TEST_FAILED;
}

TEST(AppContainerLaunchTest, CheckLPACACE) {
  if (!features::IsAppContainerSandboxSupported())
    return;
  TestRunner runner;
  AddNetworkAppContainerPolicy(runner.GetPolicy());

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"LoadDLL"));

  AppContainerBase::Delete(GetAppContainerProfileName().c_str());
}

TEST(AppContainerLaunchTest, IsAppContainer) {
  if (!features::IsAppContainerSandboxSupported())
    return;
  TestRunner runner;
  AddNetworkAppContainerPolicy(runner.GetPolicy());

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIsAppContainer"));

  AppContainerBase::Delete(GetAppContainerProfileName().c_str());
}

TEST(AppContainerLaunchTest, IsNotAppContainer) {
  TestRunner runner;

  EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(L"CheckIsAppContainer"));
}

SBOX_TESTS_COMMAND int CreateTempFileInAppContainer(int argc, wchar_t** argv) {
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
  TestRunner runner;
  std::wstring package_name = GenerateRandomPackageName();
  ASSERT_EQ(SBOX_ALL_OK,
            runner.GetPolicy()->GetConfig()->AddAppContainerProfile(
                package_name.c_str()));
  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->GetConfig()->SetTokenLevel(
                             USER_UNPROTECTED, USER_UNPROTECTED));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"CreateTempFileInAppContainer"));
  EXPECT_TRUE(AppContainerBase::Delete(package_name.c_str()));
}

TEST(LowBoxTest, ChildProcessMitigationLowBox) {
  if (!features::IsAppContainerSandboxSupported()) {
    return;
  }

  TestRunner runner(JobLevel::kUnprotected, USER_UNPROTECTED, USER_UNPROTECTED);

  EXPECT_EQ(SBOX_ALL_OK,
            runner.GetPolicy()->GetConfig()->SetLowBox(kAppContainerSid));

  // Now set the job level to be <= JobLevel::kLimitedUser
  // and ensure we can no longer create a child process.
  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->GetConfig()->SetJobLevel(
                             JobLevel::kLimitedUser, 0));

  base::FilePath cmd;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &cmd));
  cmd = cmd.Append(L"calc.exe");

  std::wstring test_command = L"TestChildProcess \"";
  test_command += cmd.value().c_str();
  test_command += L"\" false";

  EXPECT_EQ(SBOX_TEST_SECOND_ERROR, runner.RunTest(test_command.c_str()));
}

}  // namespace sandbox
