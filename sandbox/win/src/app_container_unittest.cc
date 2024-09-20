// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <windows.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/access_control_list.h"
#include "base/win/security_descriptor.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "build/build_config.h"
#include "sandbox/features.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/security_capabilities.h"
#include "sandbox/win/src/win_utils.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

bool ValidSecurityCapabilities(
    PSECURITY_CAPABILITIES security_capabilities,
    const base::win::Sid& package_sid,
    const std::vector<base::win::Sid>& capabilities) {
  if (!security_capabilities)
    return false;

  if (!package_sid.Equal(security_capabilities->AppContainerSid)) {
    return false;
  }

  // If empty then count and list of capabilities should be 0 and nullptr.
  if (capabilities.empty() && !security_capabilities->CapabilityCount &&
      !security_capabilities->Capabilities) {
    return true;
  }

  if (!security_capabilities->Capabilities)
    return false;

  if (security_capabilities->CapabilityCount != capabilities.size())
    return false;

  for (DWORD index = 0; index < security_capabilities->CapabilityCount;
       ++index) {
    if (!capabilities[index].Equal(
            security_capabilities->Capabilities[index].Sid)) {
      return false;
    }
    if (security_capabilities->Capabilities[index].Attributes !=
        SE_GROUP_ENABLED) {
      return false;
    }
  }

  return true;
}

bool CompareSidVectors(const std::vector<base::win::Sid>& left,
                       const std::vector<base::win::Sid>& right) {
  if (left.size() != right.size())
    return false;
  auto left_interator = left.cbegin();
  auto right_interator = right.cbegin();
  while (left_interator != left.cend()) {
    if (*left_interator != *right_interator)
      return false;
    ++left_interator;
    ++right_interator;
  }
  return true;
}

bool GetProfilePath(const std::wstring& package_name,
                    base::FilePath* profile_path) {
  base::FilePath local_app_data;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &local_app_data))
    return false;
  *profile_path = local_app_data.Append(L"Packages").Append(package_name);
  return true;
}

bool ProfileExist(const std::wstring& package_name) {
  base::FilePath profile_path;
  if (!GetProfilePath(package_name, &profile_path))
    return false;
  return base::PathExists(profile_path);
}

bool FindAce(const std::optional<base::win::AccessControlList>& acl,
             DWORD ace_type,
             const base::win::Sid& sid,
             DWORD flags,
             DWORD mask) {
  if (!acl || acl->is_null()) {
    return false;
  }
  PACL pacl = acl->get();
  for (DWORD ace_index = 0; ace_index < pacl->AceCount; ++ace_index) {
    PACCESS_ALLOWED_ACE ace = nullptr;
    if (::GetAce(pacl, ace_index, reinterpret_cast<LPVOID*>(&ace)) &&
        ace->Header.AceType == ace_type &&
        (ace->Header.AceFlags & flags) == flags && ace->Mask == mask &&
        sid.Equal(&ace->SidStart)) {
      return true;
    }
  }
  return false;
}

void CheckProfileDirectorySecurity(const base::FilePath& path,
                                   const base::win::Sid& package_sid) {
  DWORD flags = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromFile(
          path, DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION);
  ASSERT_TRUE(sd);
  EXPECT_TRUE(FindAce(sd->dacl(), ACCESS_ALLOWED_ACE_TYPE, package_sid, flags,
                      FILE_ALL_ACCESS))
      << path.value();
  EXPECT_TRUE(
      FindAce(sd->sacl(), SYSTEM_MANDATORY_LABEL_ACE_TYPE,
              base::win::Sid::FromIntegrityLevel(SECURITY_MANDATORY_LOW_RID),
              flags, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP))
      << path.value();
}

void CheckProfileDirectoryLayout(const AppContainerBase* profile) {
  base::FilePath profile_path;
  ASSERT_TRUE(GetProfilePath(profile->GetPackageName(), &profile_path));
  ASSERT_TRUE(base::DirectoryExists(profile_path));
  base::FilePath ac_path = profile_path.Append(L"AC");
  ASSERT_TRUE(base::DirectoryExists(ac_path));
  CheckProfileDirectorySecurity(ac_path, profile->GetPackageSid());
  base::FilePath tmp_path = ac_path.Append(L"Temp");
  ASSERT_TRUE(base::DirectoryExists(tmp_path));
  CheckProfileDirectorySecurity(tmp_path, profile->GetPackageSid());
}

std::wstring GenerateRandomPackageName() {
  return base::ASCIIToWide(base::StringPrintf(
      "%016" PRIX64 "%016" PRIX64, base::RandUint64(), base::RandUint64()));
}

base::win::SecurityDescriptor::SelfRelative CreateSdWithSid(
    const base::win::Sid& sid) {
  base::win::SecurityDescriptor sd;
  CHECK(sd.SetDaclEntry(base::win::WellKnownSid::kWorld,
                        base::win::SecurityAccessMode::kGrant, GENERIC_ALL, 0));
  CHECK(sd.SetDaclEntry(sid, base::win::SecurityAccessMode::kGrant, GENERIC_ALL,
                        0));
  return *sd.ToSelfRelative();
}

base::win::SecurityDescriptor::SelfRelative CreateSdWithSid(
    base::win::WellKnownSid known_sid) {
  return CreateSdWithSid(base::win::Sid(known_sid));
}

void AccessCheckFile(AppContainer* container,
                     const base::FilePath& path,
                     const base::win::Sid& sid,
                     DWORD desired_access,
                     DWORD expected_access,
                     BOOL expected_status) {
  auto sd = CreateSdWithSid(sid);
  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), sd.get(), FALSE};
  base::win::ScopedHandle file_handle(::CreateFile(
      path.value().c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, &sa,
      CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, nullptr));

  ASSERT_TRUE(file_handle.is_valid());
  DWORD granted_access;
  BOOL access_status;
  ASSERT_TRUE(container->AccessCheck(
      path.value().c_str(), base::win::SecurityObjectType::kFile,
      desired_access, &granted_access, &access_status));
  ASSERT_EQ(expected_status, access_status);
  if (access_status)
    ASSERT_EQ(expected_access, granted_access);
}

void AccessCheckFile(AppContainer* container,
                     const base::FilePath& path,
                     base::win::WellKnownSid known_sid,
                     DWORD desired_access,
                     DWORD expected_access,
                     BOOL expected_status) {
  AccessCheckFile(container, path, base::win::Sid(known_sid), desired_access,
                  expected_access, expected_status);
}

void AccessCheckFile(AppContainer* container,
                     const base::FilePath& path,
                     base::win::WellKnownCapability known_cap,
                     DWORD desired_access,
                     DWORD expected_access,
                     BOOL expected_status) {
  AccessCheckFile(container, path, base::win::Sid(known_cap), desired_access,
                  expected_access, expected_status);
}

void CheckDaclForPackageSid(HANDLE token,
                            const base::win::Sid& package_sid,
                            bool package_sid_required) {
  auto sd = *base::win::SecurityDescriptor::FromHandle(
      token, base::win::SecurityObjectType::kKernel, DACL_SECURITY_INFORMATION);

  EXPECT_EQ(package_sid_required,
            IsSidInDacl(*sd.dacl(), true, TOKEN_ALL_ACCESS, package_sid));
  EXPECT_NE(package_sid_required,
            IsSidInDacl(*sd.dacl(), true, TOKEN_ALL_ACCESS,
                        base::win::Sid(
                            base::win::WellKnownSid::kAllApplicationPackages)));
}

void CheckLowBoxToken(AppContainerBase* container,
                      const base::win::AccessToken& base_token,
                      bool impersonation,
                      size_t expected_cap_count) {
  std::optional<base::win::AccessToken> token =
      impersonation ? container->BuildImpersonationToken(base_token)
                    : container->BuildPrimaryToken(base_token);
  ASSERT_TRUE(token);
  EXPECT_EQ(token->User(), base_token.User());
  EXPECT_EQ(base::win::GetGrantedAccess(token->get()), DWORD{TOKEN_ALL_ACCESS});
  EXPECT_TRUE(token->IsAppContainer());
  EXPECT_EQ(impersonation, token->IsImpersonation());
  EXPECT_FALSE(token->IsIdentification());
  EXPECT_EQ(token->AppContainerSid(), container->GetPackageSid());
  const std::vector<base::win::Sid>& check_capabilities =
      impersonation ? container->GetImpersonationCapabilities()
                    : container->GetCapabilities();
  auto capabilities = token->Capabilities();
  ASSERT_EQ(capabilities.size(), check_capabilities.size());
  EXPECT_EQ(expected_cap_count, capabilities.size());
  for (size_t index = 0; index < capabilities.size(); ++index) {
    EXPECT_EQ(capabilities[index].GetAttributes(), DWORD{SE_GROUP_ENABLED});
    EXPECT_EQ(capabilities[index].GetSid(), check_capabilities[index]);
  }
  CheckDaclForPackageSid(token->get(), container->GetPackageSid(), true);
}

}  // namespace

TEST(AppContainerTest, SecurityCapabilities) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  // This isn't a valid package SID but it doesn't matter for this test.
  base::win::Sid package_sid(base::win::WellKnownSid::kNull);

  std::vector<base::win::Sid> capabilities;
  SecurityCapabilities no_capabilities(package_sid);
  EXPECT_TRUE(
      ValidSecurityCapabilities(&no_capabilities, package_sid, capabilities));

  capabilities.emplace_back(base::win::WellKnownSid::kWorld);
  SecurityCapabilities one_capability(package_sid, capabilities);
  EXPECT_TRUE(
      ValidSecurityCapabilities(&one_capability, package_sid, capabilities));

  capabilities.emplace_back(base::win::WellKnownSid::kNetwork);
  SecurityCapabilities two_capabilities(package_sid, capabilities);
  EXPECT_TRUE(
      ValidSecurityCapabilities(&two_capabilities, package_sid, capabilities));
}

TEST(AppContainerTest, CreateAndDeleteAppContainerProfile) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  std::wstring package_name = GenerateRandomPackageName();
  EXPECT_FALSE(ProfileExist(package_name));
  std::unique_ptr<AppContainerBase> profile_container =
      AppContainerBase::CreateProfile(package_name.c_str(), L"Name");
  ASSERT_NE(nullptr, profile_container.get());
  EXPECT_TRUE(ProfileExist(package_name));
  CheckProfileDirectoryLayout(profile_container.get());
  EXPECT_TRUE(AppContainerBase::Delete(package_name.c_str()));
  EXPECT_FALSE(ProfileExist(package_name));
}

TEST(AppContainerTest, CreateAndOpenAppContainer) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  std::wstring package_name = GenerateRandomPackageName();
  EXPECT_FALSE(ProfileExist(package_name));
  std::unique_ptr<AppContainerBase> profile_container =
      AppContainerBase::CreateProfile(package_name.c_str(), L"Name");
  ASSERT_NE(nullptr, profile_container.get());
  EXPECT_TRUE(ProfileExist(package_name));
  CheckProfileDirectoryLayout(profile_container.get());
  std::unique_ptr<AppContainerBase> open_container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, open_container.get());
  EXPECT_TRUE(::EqualSid(profile_container->GetPackageSid().GetPSID(),
                         open_container->GetPackageSid().GetPSID()));
  EXPECT_TRUE(AppContainerBase::Delete(package_name.c_str()));
  EXPECT_FALSE(ProfileExist(package_name));
  std::unique_ptr<AppContainerBase> open_container2 =
      AppContainerBase::Open(package_name.c_str());
  EXPECT_FALSE(ProfileExist(package_name));
}

TEST(AppContainerTest, ReOpenAppContainerProfile) {
  if (!features::IsAppContainerSandboxSupported()) {
    return;
  }
  std::wstring package_name = GenerateRandomPackageName();
  EXPECT_FALSE(ProfileExist(package_name));
  std::unique_ptr<AppContainerBase> profile_container =
      AppContainerBase::CreateProfile(package_name.c_str(), L"Name");
  ASSERT_NE(nullptr, profile_container.get());
  EXPECT_TRUE(ProfileExist(package_name));
  CheckProfileDirectoryLayout(profile_container.get());
  std::unique_ptr<AppContainerBase> open_container =
      AppContainerBase::CreateProfile(package_name.c_str(), L"Name");
  ASSERT_NE(nullptr, open_container.get());
  EXPECT_EQ(profile_container->GetPackageSid(),
            open_container->GetPackageSid());
  EXPECT_TRUE(AppContainerBase::Delete(package_name.c_str()));
  EXPECT_FALSE(ProfileExist(package_name));
}

TEST(AppContainerTest, SetLowPrivilegeAppContainer) {
  if (!features::IsAppContainerSandboxSupported())
    return;
  std::wstring package_name = GenerateRandomPackageName();
  std::unique_ptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, container.get());
  container->SetEnableLowPrivilegeAppContainer(true);
  EXPECT_TRUE(container->GetEnableLowPrivilegeAppContainer());
}

TEST(AppContainerTest, OpenAppContainerAndGetSecurityCapabilities) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  std::wstring package_name = GenerateRandomPackageName();
  std::unique_ptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, container.get());

  std::vector<base::win::Sid> capabilities;
  auto no_capabilities = container->GetSecurityCapabilities();
  ASSERT_TRUE(ValidSecurityCapabilities(
      no_capabilities.get(), container->GetPackageSid(), capabilities));

  container->AddCapability(L"FakeCapability");
  capabilities.push_back(
      base::win::Sid::FromNamedCapability(L"FakeCapability"));

  container->AddCapability(base::win::WellKnownCapability::kInternetClient);
  capabilities.emplace_back(base::win::WellKnownCapability::kInternetClient);
  const wchar_t kSddlSid[] = L"S-1-15-3-1";
  ASSERT_TRUE(container->AddCapabilitySddl(kSddlSid));
  capabilities.push_back(*base::win::Sid::FromSddlString(kSddlSid));
  auto with_capabilities = container->GetSecurityCapabilities();
  ASSERT_TRUE(ValidSecurityCapabilities(
      with_capabilities.get(), container->GetPackageSid(), capabilities));
}

TEST(AppContainerTest, AccessCheckFile) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  // We don't need a valid profile to do the access check tests.
  std::wstring package_name = GenerateRandomPackageName();
  std::unique_ptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  container->AddCapability(base::win::WellKnownCapability::kInternetClient);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().Append(package_name);

  AccessCheckFile(container.get(), path, base::win::WellKnownSid::kNull,
                  FILE_READ_DATA, 0, FALSE);
  AccessCheckFile(container.get(), path,
                  base::win::WellKnownSid::kAllApplicationPackages,
                  FILE_READ_DATA, FILE_READ_DATA, TRUE);
  AccessCheckFile(container.get(), path, container->GetPackageSid(),
                  FILE_READ_DATA, FILE_READ_DATA, TRUE);
  AccessCheckFile(container.get(), path,
                  base::win::WellKnownCapability::kInternetClient,
                  FILE_READ_DATA, FILE_READ_DATA, TRUE);

  // Check mapping generic access rights.
  AccessCheckFile(container.get(), path,
                  base::win::WellKnownSid::kAllApplicationPackages,
                  GENERIC_READ | GENERIC_EXECUTE,
                  FILE_GENERIC_READ | FILE_GENERIC_EXECUTE, TRUE);
  if (!features::IsAppContainerSandboxSupported())
    return;
  container->SetEnableLowPrivilegeAppContainer(true);
  AccessCheckFile(container.get(), path,
                  base::win::WellKnownSid::kAllApplicationPackages,
                  FILE_READ_DATA, 0, FALSE);
  AccessCheckFile(container.get(), path,
                  base::win::WellKnownSid::kAllRestrictedApplicationPackages,
                  FILE_READ_DATA, FILE_READ_DATA, TRUE);
}

TEST(AppContainerTest, AccessCheckRegistry) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  // We don't need a valid profile to do the access check tests.
  std::wstring package_name = GenerateRandomPackageName();
  std::unique_ptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  // Ensure the key doesn't exist.
  RegDeleteKey(HKEY_CURRENT_USER, package_name.c_str());
  auto sd = CreateSdWithSid(base::win::WellKnownSid::kAllApplicationPackages);
  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), sd.get(), FALSE};
  HKEY key_handle;
  ASSERT_EQ(ERROR_SUCCESS,
            RegCreateKeyEx(HKEY_CURRENT_USER, package_name.c_str(), 0, nullptr,
                           REG_OPTION_VOLATILE, KEY_ALL_ACCESS, &sa,
                           &key_handle, nullptr));
  base::win::ScopedHandle key(key_handle);
  std::wstring key_name = L"CURRENT_USER\\";
  key_name += package_name;
  DWORD granted_access;
  BOOL access_status;

  ASSERT_TRUE(container->AccessCheck(
      key_name.c_str(), base::win::SecurityObjectType::kRegistry,
      KEY_QUERY_VALUE, &granted_access, &access_status));
  ASSERT_TRUE(access_status);
  ASSERT_EQ(DWORD{KEY_QUERY_VALUE}, granted_access);
  RegDeleteKey(HKEY_CURRENT_USER, package_name.c_str());
}

TEST(AppContainerTest, ImpersonationCapabilities) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  std::wstring package_name = GenerateRandomPackageName();
  std::unique_ptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, container.get());

  std::vector<base::win::Sid> capabilities;
  std::vector<base::win::Sid> impersonation_capabilities;

  container->AddCapability(base::win::WellKnownCapability::kInternetClient);
  capabilities.emplace_back(base::win::WellKnownCapability::kInternetClient);
  impersonation_capabilities.emplace_back(
      base::win::WellKnownCapability::kInternetClient);

  ASSERT_TRUE(CompareSidVectors(container->GetCapabilities(), capabilities));
  ASSERT_TRUE(CompareSidVectors(container->GetImpersonationCapabilities(),
                                impersonation_capabilities));

  container->AddImpersonationCapability(
      base::win::WellKnownCapability::kPrivateNetworkClientServer);
  impersonation_capabilities.emplace_back(
      base::win::WellKnownCapability::kPrivateNetworkClientServer);

  container->AddImpersonationCapability(L"FakeCapability");
  impersonation_capabilities.push_back(
      base::win::Sid::FromNamedCapability(L"FakeCapability"));

  const wchar_t kSddlSid[] = L"S-1-15-3-1";
  ASSERT_TRUE(container->AddImpersonationCapabilitySddl(kSddlSid));
  impersonation_capabilities.push_back(
      *base::win::Sid::FromSddlString(kSddlSid));
  ASSERT_TRUE(CompareSidVectors(container->GetCapabilities(), capabilities));
  ASSERT_TRUE(CompareSidVectors(container->GetImpersonationCapabilities(),
                                impersonation_capabilities));
}

TEST(AppContainerTest, BuildImpersonationToken) {
  if (!features::IsAppContainerSandboxSupported()) {
    return;
  }
  std::optional<base::win::AccessToken> base_token =
      base::win::AccessToken::FromCurrentProcess(
          /*impersonation=*/false, TOKEN_DUPLICATE);
  ASSERT_TRUE(base_token);
  std::wstring package_name = GenerateRandomPackageName();
  std::unique_ptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, container.get());

  CheckLowBoxToken(container.get(), *base_token, true, 0);
  container->AddCapability(base::win::WellKnownCapability::kInternetClient);
  container->AddImpersonationCapability(
      base::win::WellKnownCapability::kPrivateNetworkClientServer);
  CheckLowBoxToken(container.get(), *base_token, true, 2);
}

TEST(AppContainerTest, BuildPrimaryToken) {
  if (!features::IsAppContainerSandboxSupported()) {
    return;
  }
  std::optional<base::win::AccessToken> base_token =
      base::win::AccessToken::FromCurrentProcess(
          /*impersonation=*/false, TOKEN_DUPLICATE);
  ASSERT_TRUE(base_token);
  std::wstring package_name = GenerateRandomPackageName();
  std::unique_ptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, container.get());

  CheckLowBoxToken(container.get(), *base_token, false, 0);
  container->AddCapability(base::win::WellKnownCapability::kInternetClient);
  container->AddImpersonationCapability(
      base::win::WellKnownCapability::kPrivateNetworkClientServer);
  CheckLowBoxToken(container.get(), *base_token, false, 1);
}

}  // namespace sandbox
