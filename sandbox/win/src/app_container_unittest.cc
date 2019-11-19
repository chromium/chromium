// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <Sddl.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/app_container_profile_base.h"
#include "sandbox/win/src/security_capabilities.h"
#include "sandbox/win/src/sid.h"
#include "sandbox/win/src/win_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

bool ValidSecurityCapabilities(PSECURITY_CAPABILITIES security_capabilities,
                               const Sid& package_sid,
                               const std::vector<Sid>& capabilities) {
  if (!security_capabilities)
    return false;

  if (!::EqualSid(package_sid.GetPSID(),
                  security_capabilities->AppContainerSid)) {
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
    if (!::EqualSid(capabilities[index].GetPSID(),
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

bool CompareSidVectors(const std::vector<Sid>& left,
                       const std::vector<Sid>& right) {
  if (left.size() != right.size())
    return false;
  auto left_interator = left.cbegin();
  auto right_interator = right.cbegin();
  while (left_interator != left.cend()) {
    if (!::EqualSid(left_interator->GetPSID(), right_interator->GetPSID()))
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

std::wstring GenerateRandomPackageName() {
  return base::StringPrintf(L"%016lX%016lX", base::RandUint64(),
                            base::RandUint64());
}

class SECURITY_ATTRIBUTES_SDDL : public SECURITY_ATTRIBUTES {
 public:
  SECURITY_ATTRIBUTES_SDDL(LPCWSTR sddl) : SECURITY_ATTRIBUTES() {
    nLength = sizeof(SECURITY_ATTRIBUTES);
    if (!::ConvertStringSecurityDescriptorToSecurityDescriptor(
            sddl, SDDL_REVISION_1, &lpSecurityDescriptor, nullptr)) {
      lpSecurityDescriptor = nullptr;
    }
  }

  ~SECURITY_ATTRIBUTES_SDDL() {
    if (lpSecurityDescriptor)
      ::LocalFree(lpSecurityDescriptor);
  }

  bool IsValid() { return lpSecurityDescriptor != nullptr; }
};

std::wstring CreateSddlWithSid(const Sid& sid) {
  std::wstring sddl_string;
  if (!sid.ToSddlString(&sddl_string))
    return L"";
  std::wstring base_sddl = L"D:(A;;GA;;;WD)(A;;GA;;;";
  return base_sddl + sddl_string + L")";
}

void AccessCheckFile(AppContainerProfile* profile,
                     const base::FilePath& path,
                     const Sid& sid,
                     DWORD desired_access,
                     DWORD expected_access,
                     BOOL expected_status) {
  SECURITY_ATTRIBUTES_SDDL sa(CreateSddlWithSid(sid).c_str());
  ASSERT_TRUE(sa.IsValid());
  base::win::ScopedHandle file_handle(::CreateFile(
      path.value().c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, &sa,
      CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, nullptr));

  ASSERT_TRUE(file_handle.IsValid());
  DWORD granted_access;
  BOOL access_status;
  ASSERT_TRUE(profile->AccessCheck(path.value().c_str(), SE_FILE_OBJECT,
                                   desired_access, &granted_access,
                                   &access_status));
  ASSERT_EQ(expected_status, access_status);
  if (access_status)
    ASSERT_EQ(expected_access, granted_access);
}

}  // namespace

TEST(AppContainerTest, SecurityCapabilities) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  // This isn't a valid package SID but it doesn't matter for this test.
  Sid package_sid(::WinNullSid);

  std::vector<Sid> capabilities;
  SecurityCapabilities no_capabilities(package_sid);
  EXPECT_TRUE(
      ValidSecurityCapabilities(&no_capabilities, package_sid, capabilities));

  capabilities.push_back(::WinWorldSid);
  SecurityCapabilities one_capability(package_sid, capabilities);
  EXPECT_TRUE(
      ValidSecurityCapabilities(&one_capability, package_sid, capabilities));

  capabilities.push_back(::WinLocalSid);
  SecurityCapabilities two_capabilities(package_sid, capabilities);
  EXPECT_TRUE(
      ValidSecurityCapabilities(&two_capabilities, package_sid, capabilities));
}

TEST(AppContainerTest, CreateAndDeleteAppContainerProfile) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring package_name = GenerateRandomPackageName();
  EXPECT_FALSE(ProfileExist(package_name));
  scoped_refptr<AppContainerProfileBase> profile =
      AppContainerProfileBase::Create(package_name.c_str(), L"Name",
                                      L"Description");
  ASSERT_NE(nullptr, profile.get());
  EXPECT_TRUE(ProfileExist(package_name));
  EXPECT_TRUE(AppContainerProfileBase::Delete(package_name.c_str()));
  EXPECT_FALSE(ProfileExist(package_name));
}

TEST(AppContainerTest, CreateAndOpenAppContainerProfile) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring package_name = GenerateRandomPackageName();
  EXPECT_FALSE(ProfileExist(package_name));
  scoped_refptr<AppContainerProfileBase> profile =
      AppContainerProfileBase::Create(package_name.c_str(), L"Name",
                                      L"Description");
  ASSERT_NE(nullptr, profile.get());
  EXPECT_TRUE(ProfileExist(package_name));
  scoped_refptr<AppContainerProfileBase> open_profile =
      AppContainerProfileBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, profile.get());
  EXPECT_TRUE(::EqualSid(profile->GetPackageSid().GetPSID(),
                         open_profile->GetPackageSid().GetPSID()));
  EXPECT_TRUE(AppContainerProfileBase::Delete(package_name.c_str()));
  EXPECT_FALSE(ProfileExist(package_name));
  scoped_refptr<AppContainerProfileBase> open_profile2 =
      AppContainerProfileBase::Open(package_name.c_str());
  EXPECT_FALSE(ProfileExist(package_name));
}

TEST(AppContainerTest, SetLowPrivilegeAppContainer) {
  // LPAC first supported in RS1.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerProfileBase> profile =
      AppContainerProfileBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, profile.get());
  profile->SetEnableLowPrivilegeAppContainer(true);
  EXPECT_TRUE(profile->GetEnableLowPrivilegeAppContainer());
}

TEST(AppContainerTest, OpenAppContainerProfileAndGetSecurityCapabilities) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerProfileBase> profile =
      AppContainerProfileBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, profile.get());

  std::vector<Sid> capabilities;
  auto no_capabilities = profile->GetSecurityCapabilities();
  ASSERT_TRUE(ValidSecurityCapabilities(
      no_capabilities.get(), profile->GetPackageSid(), capabilities));

  // No support for named capabilities prior to Win10.
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    ASSERT_TRUE(profile->AddCapability(L"FakeCapability"));
    capabilities.push_back(Sid::FromNamedCapability(L"FakeCapability"));
  }

  ASSERT_TRUE(profile->AddCapability(kInternetClient));
  capabilities.push_back(Sid::FromKnownCapability(kInternetClient));
  const wchar_t kSddlSid[] = L"S-1-15-3-1";
  ASSERT_TRUE(profile->AddCapabilitySddl(kSddlSid));
  capabilities.push_back(Sid::FromSddlString(kSddlSid));
  auto with_capabilities = profile->GetSecurityCapabilities();
  ASSERT_TRUE(ValidSecurityCapabilities(
      with_capabilities.get(), profile->GetPackageSid(), capabilities));
}

TEST(AppContainerTest, GetResources) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerProfileBase> profile =
      AppContainerProfileBase::Create(package_name.c_str(), L"Name",
                                      L"Description");
  ASSERT_NE(nullptr, profile.get());
  base::win::ScopedHandle key;
  EXPECT_TRUE(profile->GetRegistryLocation(KEY_READ, &key));
  EXPECT_TRUE(key.IsValid());
  key.Close();
  base::FilePath path;
  EXPECT_TRUE(profile->GetFolderPath(&path));
  EXPECT_TRUE(base::PathExists(path));
  base::FilePath pipe_path;
  EXPECT_TRUE(profile->GetPipePath(package_name.c_str(), &pipe_path));
  base::win::ScopedHandle pipe_handle;
  pipe_handle.Set(::CreateNamedPipe(
      pipe_path.value().c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE,
      PIPE_UNLIMITED_INSTANCES, 0, 0, 0, nullptr));
  EXPECT_TRUE(pipe_handle.IsValid());
  EXPECT_TRUE(AppContainerProfileBase::Delete(package_name.c_str()));
}

TEST(AppContainerTest, AccessCheckFile) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  // We don't need a valid profile to do the access check tests.
  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerProfileBase> profile =
      AppContainerProfileBase::Open(package_name.c_str());
  profile->AddCapability(kInternetClient);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = temp_dir.GetPath().Append(package_name);

  AccessCheckFile(profile.get(), path, ::WinNullSid, FILE_READ_DATA, 0, FALSE);
  AccessCheckFile(profile.get(), path, ::WinBuiltinAnyPackageSid,
                  FILE_READ_DATA, FILE_READ_DATA, TRUE);
  AccessCheckFile(profile.get(), path, profile->GetPackageSid(), FILE_READ_DATA,
                  FILE_READ_DATA, TRUE);
  AccessCheckFile(profile.get(), path,
                  Sid::FromKnownCapability(kInternetClient), FILE_READ_DATA,
                  FILE_READ_DATA, TRUE);

  // Check mapping generic access rights.
  AccessCheckFile(profile.get(), path, ::WinBuiltinAnyPackageSid,
                  GENERIC_READ | GENERIC_EXECUTE,
                  FILE_GENERIC_READ | FILE_GENERIC_EXECUTE, TRUE);
  // No support for LPAC less than Win10 RS1.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;
  profile->SetEnableLowPrivilegeAppContainer(true);
  AccessCheckFile(profile.get(), path, ::WinBuiltinAnyPackageSid,
                  FILE_READ_DATA, 0, FALSE);
  AccessCheckFile(profile.get(), path, Sid::AllRestrictedApplicationPackages(),
                  FILE_READ_DATA, FILE_READ_DATA, TRUE);
}

TEST(AppContainerTest, AccessCheckRegistry) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  // We don't need a valid profile to do the access check tests.
  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerProfileBase> profile =
      AppContainerProfileBase::Open(package_name.c_str());
  // Ensure the key doesn't exist.
  RegDeleteKey(HKEY_CURRENT_USER, package_name.c_str());
  SECURITY_ATTRIBUTES_SDDL sa(
      CreateSddlWithSid(::WinBuiltinAnyPackageSid).c_str());
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

  ASSERT_TRUE(profile->AccessCheck(key_name.c_str(), SE_REGISTRY_KEY,
                                   KEY_QUERY_VALUE, &granted_access,
                                   &access_status));
  ASSERT_TRUE(access_status);
  ASSERT_EQ(DWORD{KEY_QUERY_VALUE}, granted_access);
  RegDeleteKey(HKEY_CURRENT_USER, package_name.c_str());
}

TEST(AppContainerTest, ImpersonationCapabilities) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerProfileBase> profile =
      AppContainerProfileBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, profile.get());

  std::vector<Sid> capabilities;
  std::vector<Sid> impersonation_capabilities;

  ASSERT_TRUE(profile->AddCapability(kInternetClient));
  capabilities.push_back(Sid::FromKnownCapability(kInternetClient));
  impersonation_capabilities.push_back(
      Sid::FromKnownCapability(kInternetClient));

  ASSERT_TRUE(CompareSidVectors(profile->GetCapabilities(), capabilities));
  ASSERT_TRUE(CompareSidVectors(profile->GetImpersonationCapabilities(),
                                impersonation_capabilities));

  ASSERT_TRUE(profile->AddImpersonationCapability(kPrivateNetworkClientServer));
  impersonation_capabilities.push_back(
      Sid::FromKnownCapability(kPrivateNetworkClientServer));
  // No support for named capabilities prior to Win10.
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    ASSERT_TRUE(profile->AddImpersonationCapability(L"FakeCapability"));
    impersonation_capabilities.push_back(
        Sid::FromNamedCapability(L"FakeCapability"));
  }
  const wchar_t kSddlSid[] = L"S-1-15-3-1";
  ASSERT_TRUE(profile->AddImpersonationCapabilitySddl(kSddlSid));
  impersonation_capabilities.push_back(Sid::FromSddlString(kSddlSid));
  ASSERT_TRUE(CompareSidVectors(profile->GetCapabilities(), capabilities));
  ASSERT_TRUE(CompareSidVectors(profile->GetImpersonationCapabilities(),
                                impersonation_capabilities));
}

}  // namespace sandbox
