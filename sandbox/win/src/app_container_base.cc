// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <windows.h>

#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>
#include <userenv.h>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

struct FreeSidDeleter {
  inline void operator()(void* ptr) const { ::FreeSid(ptr); }
};

GENERIC_MAPPING GetGenericMappingForType(SecurityObjectType object_type) {
  GENERIC_MAPPING generic_mapping = {};
  switch (object_type) {
    case SecurityObjectType::kFile:
      generic_mapping.GenericRead = FILE_GENERIC_READ;
      generic_mapping.GenericWrite = FILE_GENERIC_WRITE;
      generic_mapping.GenericExecute = FILE_GENERIC_EXECUTE;
      generic_mapping.GenericAll = FILE_ALL_ACCESS;
      break;
    case SecurityObjectType::kRegistry:
      generic_mapping.GenericRead = KEY_READ;
      generic_mapping.GenericWrite = KEY_WRITE;
      generic_mapping.GenericExecute = KEY_EXECUTE;
      generic_mapping.GenericAll = KEY_ALL_ACCESS;
      break;
    default:
      NOTREACHED();
      break;
  }
  return generic_mapping;
}

class ScopedImpersonation {
 public:
  explicit ScopedImpersonation(const base::win::ScopedHandle& token) {
    BOOL result = ::ImpersonateLoggedOnUser(token.Get());
    DCHECK(result);
  }

  ~ScopedImpersonation() {
    BOOL result = ::RevertToSelf();
    DCHECK(result);
  }
};

}  // namespace

// static
AppContainerBase* AppContainerBase::CreateProfile(const wchar_t* package_name,
                                                  const wchar_t* display_name,
                                                  const wchar_t* description) {
  PSID package_sid_ptr = nullptr;
  HRESULT hr = ::CreateAppContainerProfile(
      package_name, display_name, description, nullptr, 0, &package_sid_ptr);
  if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))
    return Open(package_name);

  if (FAILED(hr))
    return nullptr;
  std::unique_ptr<void, FreeSidDeleter> sid_deleter(package_sid_ptr);
  auto package_sid = base::win::Sid::FromPSID(package_sid_ptr);
  if (!package_sid)
    return nullptr;
  return new AppContainerBase(*package_sid, AppContainerType::kProfile);
}

// static
AppContainerBase* AppContainerBase::Open(const wchar_t* package_name) {
  PSID package_sid_ptr = nullptr;
  HRESULT hr = ::DeriveAppContainerSidFromAppContainerName(package_name,
                                                           &package_sid_ptr);
  if (FAILED(hr))
    return nullptr;

  std::unique_ptr<void, FreeSidDeleter> sid_deleter(package_sid_ptr);
  auto package_sid = base::win::Sid::FromPSID(package_sid_ptr);
  if (!package_sid)
    return nullptr;
  return new AppContainerBase(*package_sid, AppContainerType::kDerived);
}

// static
AppContainerBase* AppContainerBase::CreateLowbox(const wchar_t* sid) {
  auto package_sid = base::win::Sid::FromSddlString(sid);
  if (!package_sid)
    return nullptr;

  return new AppContainerBase(*package_sid, AppContainerType::kLowbox);
}

// static
bool AppContainerBase::Delete(const wchar_t* package_name) {
  return SUCCEEDED(::DeleteAppContainerProfile(package_name));
}

AppContainerBase::AppContainerBase(base::win::Sid& package_sid,
                                   AppContainerType type)
    : ref_count_(0),
      package_sid_(std::move(package_sid)),
      enable_low_privilege_app_container_(false),
      type_(type) {}

AppContainerBase::~AppContainerBase() {}

void AppContainerBase::AddRef() {
  // ref_count starts at 0 for this class so can increase from 0 (once).
  CHECK(::InterlockedIncrement(&ref_count_) > 0);
}

void AppContainerBase::Release() {
  LONG result = ::InterlockedDecrement(&ref_count_);
  CHECK(result >= 0);
  if (result == 0) {
    delete this;
  }
}

bool AppContainerBase::GetRegistryLocation(REGSAM desired_access,
                                           base::win::ScopedHandle* key) {
  base::win::ScopedHandle token;
  if (BuildLowBoxToken(&token) != SBOX_ALL_OK)
    return false;

  ScopedImpersonation impersonation(token);
  HKEY key_handle;
  if (FAILED(::GetAppContainerRegistryLocation(desired_access, &key_handle)))
    return false;
  key->Set(key_handle);
  return true;
}

bool AppContainerBase::GetFolderPath(base::FilePath* file_path) {
  auto sddl_str = package_sid_.ToSddlString();
  if (!sddl_str)
    return false;
  base::win::ScopedCoMem<wchar_t> path_str;
  if (FAILED(::GetAppContainerFolderPath(sddl_str->c_str(), &path_str)))
    return false;
  *file_path = base::FilePath(path_str.get());
  return true;
}

bool AppContainerBase::GetPipePath(const wchar_t* pipe_name,
                                   base::FilePath* pipe_path) {
  auto sddl_str = package_sid_.ToSddlString();
  if (!sddl_str)
    return false;
  *pipe_path = base::FilePath(base::StringPrintf(L"\\\\.\\pipe\\%ls\\%ls",
                                                 sddl_str->c_str(), pipe_name));
  return true;
}

bool AppContainerBase::AccessCheck(const wchar_t* object_name,
                                   SecurityObjectType object_type,
                                   DWORD desired_access,
                                   DWORD* granted_access,
                                   BOOL* access_status) {
  SE_OBJECT_TYPE native_object_type;
  switch (object_type) {
    case SecurityObjectType::kFile:
      native_object_type = SE_FILE_OBJECT;
      break;
    case SecurityObjectType::kRegistry:
      native_object_type = SE_REGISTRY_KEY;
      break;
    default:
      ::SetLastError(ERROR_INVALID_PARAMETER);
      return false;
  }

  GENERIC_MAPPING generic_mapping = GetGenericMappingForType(object_type);
  ::MapGenericMask(&desired_access, &generic_mapping);
  PSECURITY_DESCRIPTOR sd_ptr = nullptr;
  PACL dacl = nullptr;
  if (::GetNamedSecurityInfo(
          object_name, native_object_type,
          OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
              DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION,
          nullptr, nullptr, &dacl, nullptr, &sd_ptr) != ERROR_SUCCESS) {
    return false;
  }

  base::win::ScopedLocalAlloc sd = base::win::TakeLocalAlloc(sd_ptr);

  if (enable_low_privilege_app_container_) {
    base::win::Sid any_package_sid(
        base::win::WellKnownSid::kAllApplicationPackages);
    // We can't create a LPAC token directly, so modify the DACL to simulate it.
    // Set mask for ALL APPLICATION PACKAGE Sid to 0.
    for (WORD index = 0; index < dacl->AceCount; ++index) {
      PVOID temp_ace;
      if (!::GetAce(dacl, index, &temp_ace))
        return false;
      PACE_HEADER header = static_cast<PACE_HEADER>(temp_ace);
      if ((header->AceType != ACCESS_ALLOWED_ACE_TYPE) &&
          (header->AceType != ACCESS_DENIED_ACE_TYPE)) {
        continue;
      }
      // Allowed and deny aces have the same underlying structure.
      PACCESS_ALLOWED_ACE ace = static_cast<PACCESS_ALLOWED_ACE>(temp_ace);
      if (!::IsValidSid(&ace->SidStart)) {
        continue;
      }
      if (any_package_sid.Equal(&ace->SidStart)) {
        ace->Mask = 0;
      }
    }
  }

  PRIVILEGE_SET priv_set = {};
  DWORD priv_set_length = sizeof(PRIVILEGE_SET);

  base::win::ScopedHandle token;
  if (BuildLowBoxToken(&token) != SBOX_ALL_OK)
    return false;

  return !!::AccessCheck(sd.get(), token.Get(), desired_access,
                         &generic_mapping, &priv_set, &priv_set_length,
                         granted_access, access_status);
}

bool AppContainerBase::AddCapability(const wchar_t* capability_name) {
  return AddCapability(base::win::Sid::FromNamedCapability(capability_name),
                       false);
}

bool AppContainerBase::AddCapability(
    base::win::WellKnownCapability capability) {
  return AddCapability(base::win::Sid::FromKnownCapability(capability), false);
}

bool AppContainerBase::AddCapabilitySddl(const wchar_t* sddl_sid) {
  return AddCapability(base::win::Sid::FromSddlString(sddl_sid), false);
}

bool AppContainerBase::AddCapability(
    const absl::optional<base::win::Sid>& capability_sid,
    bool impersonation_only) {
  if (!capability_sid)
    return false;
  if (!impersonation_only)
    capabilities_.push_back(capability_sid->Clone());
  impersonation_capabilities_.push_back(capability_sid->Clone());
  return true;
}

bool AppContainerBase::AddImpersonationCapability(
    const wchar_t* capability_name) {
  return AddCapability(base::win::Sid::FromNamedCapability(capability_name),
                       true);
}

bool AppContainerBase::AddImpersonationCapability(
    base::win::WellKnownCapability capability) {
  return AddCapability(base::win::Sid::FromKnownCapability(capability), true);
}

bool AppContainerBase::AddImpersonationCapabilitySddl(const wchar_t* sddl_sid) {
  return AddCapability(base::win::Sid::FromSddlString(sddl_sid), true);
}

const std::vector<base::win::Sid>& AppContainerBase::GetCapabilities() {
  return capabilities_;
}

const std::vector<base::win::Sid>&
AppContainerBase::GetImpersonationCapabilities() {
  return impersonation_capabilities_;
}

const base::win::Sid& AppContainerBase::GetPackageSid() const {
  return package_sid_;
}

void AppContainerBase::SetEnableLowPrivilegeAppContainer(bool enable) {
  enable_low_privilege_app_container_ = enable;
}

bool AppContainerBase::GetEnableLowPrivilegeAppContainer() {
  return enable_low_privilege_app_container_;
}

AppContainerType AppContainerBase::GetAppContainerType() {
  return type_;
}

std::unique_ptr<SecurityCapabilities>
AppContainerBase::GetSecurityCapabilities() {
  return std::make_unique<SecurityCapabilities>(package_sid_, capabilities_);
}

ResultCode AppContainerBase::BuildLowBoxToken(
    base::win::ScopedHandle* token,
    base::win::ScopedHandle* lockdown) {
  if (type_ == AppContainerType::kLowbox) {
    if (CreateLowBoxToken(lockdown->Get(), PRIMARY,
                          GetSecurityCapabilities().get(),
                          token) != ERROR_SUCCESS) {
      return SBOX_ERROR_CANNOT_CREATE_LOWBOX_TOKEN;
    }

    if (!ReplacePackageSidInDacl(token->Get(), SecurityObjectType::kKernel,
                                 package_sid_, TOKEN_ALL_ACCESS)) {
      return SBOX_ERROR_CANNOT_MODIFY_LOWBOX_TOKEN_DACL;
    }
  } else if (CreateLowBoxToken(nullptr, IMPERSONATION,
                               GetSecurityCapabilities().get(),
                               token) != ERROR_SUCCESS) {
    return SBOX_ERROR_CANNOT_CREATE_LOWBOX_IMPERSONATION_TOKEN;
  }

  return SBOX_ALL_OK;
}

}  // namespace sandbox
