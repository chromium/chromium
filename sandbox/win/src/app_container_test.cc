// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <sddl.h>

#include <memory>
#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/app_container_profile_base.h"
#include "sandbox/win/src/sync_policy_test.h"
#include "sandbox/win/src/win_utils.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

const wchar_t kAppContainerSid[] =
    L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
    L"924012148-2839372144";

std::wstring GenerateRandomPackageName() {
  return base::StringPrintf(L"%016lX%016lX", base::RandUint64(),
                            base::RandUint64());
}

const char* TokenTypeToName(TOKEN_TYPE token_type) {
  return token_type == ::TokenPrimary ? "Primary Token" : "Impersonation Token";
}

void CheckToken(HANDLE token,
                TOKEN_TYPE token_type,
                PSECURITY_CAPABILITIES security_capabilities,
                BOOL restricted) {
  ASSERT_EQ(restricted, ::IsTokenRestricted(token))
      << TokenTypeToName(token_type);

  DWORD appcontainer;
  DWORD return_length;
  ASSERT_TRUE(::GetTokenInformation(token, ::TokenIsAppContainer, &appcontainer,
                                    sizeof(appcontainer), &return_length))
      << TokenTypeToName(token_type);
  ASSERT_TRUE(appcontainer) << TokenTypeToName(token_type);
  TOKEN_TYPE token_type_real;
  ASSERT_TRUE(::GetTokenInformation(token, ::TokenType, &token_type_real,
                                    sizeof(token_type_real), &return_length))
      << TokenTypeToName(token_type);
  ASSERT_EQ(token_type_real, token_type) << TokenTypeToName(token_type);
  if (token_type == ::TokenImpersonation) {
    SECURITY_IMPERSONATION_LEVEL imp_level;
    ASSERT_TRUE(::GetTokenInformation(token, ::TokenImpersonationLevel,
                                      &imp_level, sizeof(imp_level),
                                      &return_length))
        << TokenTypeToName(token_type);
    ASSERT_EQ(imp_level, ::SecurityImpersonation)
        << TokenTypeToName(token_type);
  }

  std::unique_ptr<Sid> package_sid;
  ASSERT_TRUE(GetTokenAppContainerSid(token, &package_sid))
      << TokenTypeToName(token_type);
  EXPECT_TRUE(::EqualSid(security_capabilities->AppContainerSid,
                         package_sid->GetPSID()))
      << TokenTypeToName(token_type);

  std::vector<SidAndAttributes> capabilities;
  ASSERT_TRUE(GetTokenGroups(token, ::TokenCapabilities, &capabilities))
      << TokenTypeToName(token_type);

  ASSERT_EQ(capabilities.size(), security_capabilities->CapabilityCount)
      << TokenTypeToName(token_type);
  for (size_t index = 0; index < capabilities.size(); ++index) {
    EXPECT_EQ(capabilities[index].GetAttributes(),
              security_capabilities->Capabilities[index].Attributes)
        << TokenTypeToName(token_type);
    EXPECT_TRUE(::EqualSid(capabilities[index].GetPSID(),
                           security_capabilities->Capabilities[index].Sid))
        << TokenTypeToName(token_type);
  }
}

void CheckProcessToken(HANDLE process,
                       PSECURITY_CAPABILITIES security_capabilities,
                       bool restricted) {
  HANDLE token_handle;
  ASSERT_TRUE(::OpenProcessToken(process, TOKEN_ALL_ACCESS, &token_handle));
  base::win::ScopedHandle token(token_handle);
  CheckToken(token_handle, ::TokenPrimary, security_capabilities, restricted);
}

void CheckThreadToken(HANDLE thread,
                      PSECURITY_CAPABILITIES security_capabilities,
                      bool restricted) {
  HANDLE token_handle;
  ASSERT_TRUE(::OpenThreadToken(thread, TOKEN_ALL_ACCESS, TRUE, &token_handle));
  base::win::ScopedHandle token(token_handle);
  CheckToken(token_handle, ::TokenImpersonation, security_capabilities,
             restricted);
}

// Check for LPAC using an access check. We could query for a security attribute
// but that's undocumented and has the potential to change.
void CheckLpacToken(HANDLE process) {
  HANDLE token_handle;
  ASSERT_TRUE(::OpenProcessToken(process, TOKEN_ALL_ACCESS, &token_handle));
  base::win::ScopedHandle token(token_handle);
  ASSERT_TRUE(
      ::DuplicateToken(token.Get(), ::SecurityImpersonation, &token_handle));
  token.Set(token_handle);
  PSECURITY_DESCRIPTOR security_desc_ptr;
  // AC is AllPackages, S-1-15-2-2 is AllRestrictedPackages. An LPAC token
  // will get granted access of 2, where as a normal AC token will get 3.
  ASSERT_TRUE(::ConvertStringSecurityDescriptorToSecurityDescriptor(
      L"O:SYG:SYD:(A;;0x3;;;WD)(A;;0x1;;;AC)(A;;0x2;;;S-1-15-2-2)",
      SDDL_REVISION_1, &security_desc_ptr, nullptr));
  std::unique_ptr<void, LocalFreeDeleter> security_desc(security_desc_ptr);
  GENERIC_MAPPING generic_mapping = {};
  PRIVILEGE_SET priv_set = {};
  DWORD priv_set_length = sizeof(PRIVILEGE_SET);
  DWORD granted_access;
  BOOL access_status;
  ASSERT_TRUE(::AccessCheck(security_desc_ptr, token.Get(), MAXIMUM_ALLOWED,
                            &generic_mapping, &priv_set, &priv_set_length,
                            &granted_access, &access_status));
  ASSERT_TRUE(access_status);
  ASSERT_EQ(DWORD{2}, granted_access);
}

class AppContainerProfileTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (base::win::GetVersion() < base::win::Version::WIN8)
      return;
    package_name_ = GenerateRandomPackageName();
    broker_services_ = GetBroker();
    policy_ = broker_services_->CreatePolicy();
    ASSERT_EQ(SBOX_ALL_OK,
              policy_->SetProcessMitigations(MITIGATION_HEAP_TERMINATE));
    ASSERT_EQ(SBOX_ALL_OK,
              policy_->AddAppContainerProfile(package_name_.c_str(), true));
    // For testing purposes we known the base class so cast directly.
    profile_ = static_cast<AppContainerProfileBase*>(
        policy_->GetAppContainerProfile().get());
  }

  void TearDown() override {
    if (scoped_process_info_.IsValid())
      ::TerminateProcess(scoped_process_info_.process_handle(), 0);
    if (profile_)
      AppContainerProfileBase::Delete(package_name_.c_str());
  }

 protected:
  void CreateProcess() {
    // Get the path to the sandboxed app.
    wchar_t prog_name[MAX_PATH] = {};
    ASSERT_NE(DWORD{0}, ::GetModuleFileNameW(nullptr, prog_name, MAX_PATH));

    PROCESS_INFORMATION process_info = {};
    ResultCode last_warning = SBOX_ALL_OK;
    DWORD last_error = 0;
    ResultCode result = broker_services_->SpawnTarget(
        prog_name, prog_name, policy_, &last_warning, &last_error,
        &process_info);
    ASSERT_EQ(SBOX_ALL_OK, result) << "Last Error: " << last_error;
    scoped_process_info_.Set(process_info);
  }

  std::wstring package_name_;
  BrokerServices* broker_services_;
  scoped_refptr<AppContainerProfileBase> profile_;
  scoped_refptr<TargetPolicy> policy_;
  base::win::ScopedProcessInformation scoped_process_info_;
};

}  // namespace


TEST(AppContainerTest, DenyOpenEventForLowBox) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  TestRunner runner(JOB_UNPROTECTED, USER_UNPROTECTED, USER_UNPROTECTED);

  EXPECT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetLowBox(kAppContainerSid));
  // Run test once, this ensures the app container directory exists, we
  // ignore the result.
  runner.RunTest(L"Event_Open f test");
  std::wstring event_name = L"AppContainerNamedObjects\\";
  event_name += kAppContainerSid;
  event_name += L"\\test";

  base::win::ScopedHandle event(
      ::CreateEvent(nullptr, false, false, event_name.c_str()));
  ASSERT_TRUE(event.IsValid());

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Event_Open f test"));
}

TEST_F(AppContainerProfileTest, CheckIncompatibleOptions) {
  if (!profile_)
    return;
  EXPECT_EQ(SBOX_ERROR_BAD_PARAMS,
            policy_->SetIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED));
  EXPECT_EQ(SBOX_ERROR_BAD_PARAMS, policy_->SetLowBox(kAppContainerSid));

  MitigationFlags expected_mitigations = 0;
  MitigationFlags expected_delayed = MITIGATION_HEAP_TERMINATE;
  sandbox::ResultCode expected_result = SBOX_ERROR_BAD_PARAMS;

  if (base::win::GetVersion() >= base::win::Version::WIN10_RS5) {
    expected_mitigations = MITIGATION_HEAP_TERMINATE;
    expected_delayed = 0;
    expected_result = SBOX_ALL_OK;
  }

  EXPECT_EQ(expected_mitigations, policy_->GetProcessMitigations());
  EXPECT_EQ(expected_delayed, policy_->GetDelayedProcessMitigations());
  EXPECT_EQ(expected_result,
            policy_->SetProcessMitigations(MITIGATION_HEAP_TERMINATE));
}

TEST_F(AppContainerProfileTest, NoCapabilities) {
  if (!profile_)
    return;

  policy_->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  policy_->SetJobLevel(JOB_NONE, 0);

  CreateProcess();
  auto security_capabilities = profile_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), FALSE);
}

TEST_F(AppContainerProfileTest, NoCapabilitiesRestricted) {
  if (!profile_)
    return;

  policy_->SetTokenLevel(USER_LOCKDOWN, USER_RESTRICTED_SAME_ACCESS);
  policy_->SetJobLevel(JOB_NONE, 0);

  CreateProcess();
  auto security_capabilities = profile_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), TRUE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), TRUE);
}

TEST_F(AppContainerProfileTest, WithCapabilities) {
  if (!profile_)
    return;

  profile_->AddCapability(kInternetClient);
  profile_->AddCapability(kInternetClientServer);
  policy_->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  policy_->SetJobLevel(JOB_NONE, 0);

  CreateProcess();
  auto security_capabilities = profile_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), FALSE);
}

TEST_F(AppContainerProfileTest, WithCapabilitiesRestricted) {
  if (!profile_)
    return;

  profile_->AddCapability(kInternetClient);
  profile_->AddCapability(kInternetClientServer);
  policy_->SetTokenLevel(USER_LOCKDOWN, USER_RESTRICTED_SAME_ACCESS);
  policy_->SetJobLevel(JOB_NONE, 0);

  CreateProcess();
  auto security_capabilities = profile_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), TRUE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), TRUE);
}

TEST_F(AppContainerProfileTest, WithImpersonationCapabilities) {
  if (!profile_)
    return;

  profile_->AddCapability(kInternetClient);
  profile_->AddCapability(kInternetClientServer);
  profile_->AddImpersonationCapability(kPrivateNetworkClientServer);
  profile_->AddImpersonationCapability(kPicturesLibrary);
  policy_->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  policy_->SetJobLevel(JOB_NONE, 0);

  CreateProcess();
  auto security_capabilities = profile_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  SecurityCapabilities impersonation_security_capabilities(
      profile_->GetPackageSid(), profile_->GetImpersonationCapabilities());
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   &impersonation_security_capabilities, FALSE);
}

TEST_F(AppContainerProfileTest, NoCapabilitiesLPAC) {
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  profile_->SetEnableLowPrivilegeAppContainer(true);
  policy_->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);
  policy_->SetJobLevel(JOB_NONE, 0);

  CreateProcess();
  auto security_capabilities = profile_->GetSecurityCapabilities();

  CheckProcessToken(scoped_process_info_.process_handle(),
                    security_capabilities.get(), FALSE);
  CheckThreadToken(scoped_process_info_.thread_handle(),
                   security_capabilities.get(), FALSE);
  CheckLpacToken(scoped_process_info_.process_handle());
}

}  // namespace sandbox
