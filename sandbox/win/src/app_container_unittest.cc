// Copyright 2017 The Chromium Authors
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
#include "base/win/sid.h"
#include "sandbox/features.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/security_capabilities.h"
#include "sandbox/win/src/win_utils.h"
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

std::wstring GenerateRandomPackageName() {
  return base::StringPrintf(L"%016lX%016lX", base::RandUint64(),
                            base::RandUint64());
}

class SECURITY_ATTRIBUTES_SDDL : public SECURITY_ATTRIBUTES {
 public:
  explicit SECURITY_ATTRIBUTES_SDDL(LPCWSTR sddl) : SECURITY_ATTRIBUTES() {
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

std::wstring CreateSddlWithSid(const base::win::Sid& sid) {
  auto sddl_string = sid.ToSddlString();
  if (!sddl_string)
    return L"";
  std::wstring base_sddl = L"D:(A;;GA;;;WD)(A;;GA;;;";
  return base_sddl + *sddl_string + L")";
}

std::wstring CreateSddlWithSid(base::win::WellKnownSid known_sid) {
  return CreateSddlWithSid(base::win::Sid(known_sid));
}

void AccessCheckFile(AppContainer* container,
                     const base::FilePath& path,
                     const base::win::Sid& sid,
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
  ASSERT_TRUE(container->AccessCheck(path.value().c_str(),
                                     SecurityObjectType::kFile, desired_access,
                                     &granted_access, &access_status));
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
  scoped_refptr<AppContainerBase> profile_container =
      AppContainerBase::CreateProfile(package_name.c_str(), L"Name",
                                      L"Description");
  ASSERT_NE(nullptr, profile_container.get());
  EXPECT_TRUE(ProfileExist(package_name));
  EXPECT_TRUE(AppContainerBase::Delete(package_name.c_str()));
  EXPECT_FALSE(ProfileExist(package_name));
}

TEST(AppContainerTest, CreateAndOpenAppContainer) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  std::wstring package_name = GenerateRandomPackageName();
  EXPECT_FALSE(ProfileExist(package_name));
  scoped_refptr<AppContainerBase> profile_container =
      AppContainerBase::CreateProfile(package_name.c_str(), L"Name",
                                      L"Description");
  ASSERT_NE(nullptr, profile_container.get());
  EXPECT_TRUE(ProfileExist(package_name));
  scoped_refptr<AppContainerBase> open_container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, open_container.get());
  EXPECT_TRUE(::EqualSid(profile_container->GetPackageSid().GetPSID(),
                         open_container->GetPackageSid().GetPSID()));
  EXPECT_TRUE(AppContainerBase::Delete(package_name.c_str()));
  EXPECT_FALSE(ProfileExist(package_name));
  scoped_refptr<AppContainerBase> open_container2 =
      AppContainerBase::Open(package_name.c_str());
  EXPECT_FALSE(ProfileExist(package_name));
}

TEST(AppContainerTest, SetLowPrivilegeAppContainer) {
  if (!features::IsAppContainerSandboxSupported())
    return;
  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, container.get());
  container->SetEnableLowPrivilegeAppContainer(true);
  EXPECT_TRUE(container->GetEnableLowPrivilegeAppContainer());
}

TEST(AppContainerTest, OpenAppContainerAndGetSecurityCapabilities) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, container.get());

  std::vector<base::win::Sid> capabilities;
  auto no_capabilities = container->GetSecurityCapabilities();
  ASSERT_TRUE(ValidSecurityCapabilities(
      no_capabilities.get(), container->GetPackageSid(), capabilities));

  ASSERT_TRUE(container->AddCapability(L"FakeCapability"));
  capabilities.push_back(
      base::win::Sid::FromNamedCapability(L"FakeCapability"));

  ASSERT_TRUE(container->AddCapability(
      base::win::WellKnownCapability::kInternetClient));
  capabilities.emplace_back(base::win::WellKnownCapability::kInternetClient);
  const wchar_t kSddlSid[] = L"S-1-15-3-1";
  ASSERT_TRUE(container->AddCapabilitySddl(kSddlSid));
  capabilities.push_back(*base::win::Sid::FromSddlString(kSddlSid));
  auto with_capabilities = container->GetSecurityCapabilities();
  ASSERT_TRUE(ValidSecurityCapabilities(
      with_capabilities.get(), container->GetPackageSid(), capabilities));
}

TEST(AppContainerTest, GetResources) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerBase> profile_container =
      AppContainerBase::CreateProfile(package_name.c_str(), L"Name",
                                      L"Description");
  ASSERT_NE(nullptr, profile_container.get());
  base::win::ScopedHandle key;
  EXPECT_TRUE(profile_container->GetRegistryLocation(KEY_READ, &key));
  EXPECT_TRUE(key.IsValid());
  key.Close();
  base::FilePath path;
  EXPECT_TRUE(profile_container->GetFolderPath(&path));
  EXPECT_TRUE(base::PathExists(path));
  base::FilePath pipe_path;
  EXPECT_TRUE(profile_container->GetPipePath(package_name.c_str(), &pipe_path));
  base::win::ScopedHandle pipe_handle;
  pipe_handle.Set(::CreateNamedPipe(
      pipe_path.value().c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE,
      PIPE_UNLIMITED_INSTANCES, 0, 0, 0, nullptr));
  EXPECT_TRUE(pipe_handle.IsValid());
  EXPECT_TRUE(AppContainerBase::Delete(package_name.c_str()));
}

TEST(AppContainerTest, AccessCheckFile) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  // We don't need a valid profile to do the access check tests.
  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerBase> container =
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
  scoped_refptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  // Ensure the key doesn't exist.
  RegDeleteKey(HKEY_CURRENT_USER, package_name.c_str());
  SECURITY_ATTRIBUTES_SDDL sa(
      CreateSddlWithSid(base::win::WellKnownSid::kAllApplicationPackages)
          .c_str());
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

  ASSERT_TRUE(
      container->AccessCheck(key_name.c_str(), SecurityObjectType::kRegistry,
                             KEY_QUERY_VALUE, &granted_access, &access_status));
  ASSERT_TRUE(access_status);
  ASSERT_EQ(DWORD{KEY_QUERY_VALUE}, granted_access);
  RegDeleteKey(HKEY_CURRENT_USER, package_name.c_str());
}

TEST(AppContainerTest, ImpersonationCapabilities) {
  if (!features::IsAppContainerSandboxSupported())
    return;

  std::wstring package_name = GenerateRandomPackageName();
  scoped_refptr<AppContainerBase> container =
      AppContainerBase::Open(package_name.c_str());
  ASSERT_NE(nullptr, container.get());

  std::vector<base::win::Sid> capabilities;
  std::vector<base::win::Sid> impersonation_capabilities;

  ASSERT_TRUE(container->AddCapability(
      base::win::WellKnownCapability::kInternetClient));
  capabilities.emplace_back(base::win::WellKnownCapability::kInternetClient);
  impersonation_capabilities.emplace_back(
      base::win::WellKnownCapability::kInternetClient);

  ASSERT_TRUE(CompareSidVectors(container->GetCapabilities(), capabilities));
  ASSERT_TRUE(CompareSidVectors(container->GetImpersonationCapabilities(),
                                impersonation_capabilities));

  ASSERT_TRUE(container->AddImpersonationCapability(
      base::win::WellKnownCapability::kPrivateNetworkClientServer));
  impersonation_capabilities.emplace_back(
      base::win::WellKnownCapability::kPrivateNetworkClientServer);

  ASSERT_TRUE(container->AddImpersonationCapability(L"FakeCapability"));
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

}  // namespace sandbox
