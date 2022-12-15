// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/acl.h"

#include <memory>

#include <windows.h>

#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>

#include "base/check.h"
#include "base/memory/free_deleter.h"
#include "base/notreached.h"
#include "base/win/access_token.h"
#include "base/win/scoped_localalloc.h"

namespace sandbox {

namespace {

ACCESS_MODE ConvertAccessMode(SecurityAccessMode access_mode) {
  switch (access_mode) {
    case SecurityAccessMode::kGrant:
      return GRANT_ACCESS;
    case SecurityAccessMode::kSet:
      return SET_ACCESS;
    case SecurityAccessMode::kDeny:
      return DENY_ACCESS;
    case SecurityAccessMode::kRevoke:
      return REVOKE_ACCESS;
    default:
      NOTREACHED();
      break;
  }
  return NOT_USED_ACCESS;
}

absl::optional<base::win::ScopedLocalAllocTyped<ACL>> AddSidToDacl(
    const base::win::Sid& sid,
    ACL* old_dacl,
    SecurityAccessMode access_mode,
    ACCESS_MASK access) {
  EXPLICIT_ACCESS new_access = {};
  new_access.grfAccessMode = ConvertAccessMode(access_mode);
  new_access.grfAccessPermissions = access;
  new_access.grfInheritance = NO_INHERITANCE;
  ::BuildTrusteeWithSid(&new_access.Trustee, sid.GetPSID());
  ACL* new_dacl = nullptr;
  if (ERROR_SUCCESS != ::SetEntriesInAcl(1, &new_access, old_dacl, &new_dacl))
    return absl::nullopt;
  DCHECK(::IsValidAcl(new_dacl));
  return base::win::TakeLocalAlloc(new_dacl);
}

SE_OBJECT_TYPE ConvertObjectType(SecurityObjectType object_type) {
  switch (object_type) {
    case SecurityObjectType::kFile:
      return SE_FILE_OBJECT;
    case SecurityObjectType::kRegistry:
      return SE_REGISTRY_KEY;
    case SecurityObjectType::kWindow:
      return SE_WINDOW_OBJECT;
    case SecurityObjectType::kKernel:
      return SE_KERNEL_OBJECT;
    default:
      NOTREACHED();
      break;
  }
  return SE_UNKNOWN_OBJECT_TYPE;
}

absl::optional<DWORD> GetIntegrityLevelValue(IntegrityLevel integrity_level) {
  switch (integrity_level) {
    case INTEGRITY_LEVEL_SYSTEM:
      return DWORD{SECURITY_MANDATORY_SYSTEM_RID};
    case INTEGRITY_LEVEL_HIGH:
      return DWORD{SECURITY_MANDATORY_HIGH_RID};
    case INTEGRITY_LEVEL_MEDIUM:
      return DWORD{SECURITY_MANDATORY_MEDIUM_RID};
    case INTEGRITY_LEVEL_MEDIUM_LOW:
      return DWORD{SECURITY_MANDATORY_MEDIUM_RID - 2048};
    case INTEGRITY_LEVEL_LOW:
      return DWORD{SECURITY_MANDATORY_LOW_RID};
    case INTEGRITY_LEVEL_BELOW_LOW:
      return DWORD{SECURITY_MANDATORY_LOW_RID - 2048};
    case INTEGRITY_LEVEL_UNTRUSTED:
      return DWORD{SECURITY_MANDATORY_UNTRUSTED_RID};
    case INTEGRITY_LEVEL_LAST:
      return absl::nullopt;
  }

  NOTREACHED();
  return absl::nullopt;
}

bool AddSidToDefaultDacl(HANDLE token,
                         const base::win::AccessToken& query_token,
                         const base::win::Sid& sid,
                         SecurityAccessMode access_mode,
                         ACCESS_MASK access) {
  if (!token)
    return false;

  absl::optional<base::win::AccessControlList> dacl = query_token.DefaultDacl();
  if (!dacl)
    return false;
  auto new_dacl = AddSidToDacl(sid, dacl->get(), access_mode, access);
  if (!new_dacl)
    return false;

  TOKEN_DEFAULT_DACL new_token_dacl = {0};
  new_token_dacl.DefaultDacl = new_dacl->get();
  return !!::SetTokenInformation(token, TokenDefaultDacl, &new_token_dacl,
                                 sizeof(new_token_dacl));
}

}  // namespace

bool AddSidToDefaultDacl(HANDLE token,
                         const base::win::Sid& sid,
                         SecurityAccessMode access_mode,
                         ACCESS_MASK access) {
  absl::optional<base::win::AccessToken> query_token =
      base::win::AccessToken::FromToken(token);
  if (!query_token)
    return false;

  return AddSidToDefaultDacl(token, *query_token, sid, access_mode, access);
}

bool AddSidToDefaultDacl(HANDLE token,
                         base::win::WellKnownSid known_sid,
                         SecurityAccessMode access_mode,
                         ACCESS_MASK access) {
  return AddSidToDefaultDacl(token, base::win::Sid(known_sid), access_mode,
                             access);
}

bool RevokeLogonSidFromDefaultDacl(HANDLE token) {
  absl::optional<base::win::AccessToken> query_token =
      base::win::AccessToken::FromToken(token);
  if (!query_token)
    return false;
  absl::optional<base::win::Sid> logon_sid = query_token->LogonId();
  if (!logon_sid)
    return ::GetLastError() == ERROR_NOT_FOUND;

  return AddSidToDefaultDacl(token, *query_token, *logon_sid,
                             SecurityAccessMode::kRevoke, 0);
}

bool AddUserSidToDefaultDacl(HANDLE token, ACCESS_MASK access) {
  absl::optional<base::win::AccessToken> query_token =
      base::win::AccessToken::FromToken(token);
  if (!query_token)
    return false;
  return AddSidToDefaultDacl(token, *query_token, query_token->User(),
                             SecurityAccessMode::kGrant, access);
}

bool AddKnownSidToObject(HANDLE object,
                         SecurityObjectType object_type,
                         const base::win::Sid& sid,
                         SecurityAccessMode access_mode,
                         ACCESS_MASK access) {
  PSECURITY_DESCRIPTOR descriptor_ptr = nullptr;
  PACL old_dacl = nullptr;
  SE_OBJECT_TYPE native_object_type = ConvertObjectType(object_type);
  if (native_object_type == SE_UNKNOWN_OBJECT_TYPE)
    return false;

  if (ERROR_SUCCESS != ::GetSecurityInfo(object, native_object_type,
                                         DACL_SECURITY_INFORMATION, nullptr,
                                         nullptr, &old_dacl, nullptr,
                                         &descriptor_ptr)) {
    return false;
  }

  auto descriptor = base::win::TakeLocalAlloc(descriptor_ptr);
  auto new_dacl = AddSidToDacl(sid, old_dacl, access_mode, access);
  if (!new_dacl)
    return false;

  return ::SetSecurityInfo(object, native_object_type,
                           DACL_SECURITY_INFORMATION, nullptr, nullptr,
                           new_dacl->get(), nullptr) == ERROR_SUCCESS;
}

bool AddKnownSidToObject(HANDLE object,
                         SecurityObjectType object_type,
                         base::win::WellKnownSid known_sid,
                         SecurityAccessMode access_mode,
                         ACCESS_MASK access) {
  return AddKnownSidToObject(object, object_type, base::win::Sid(known_sid),
                             access_mode, access);
}

bool ReplacePackageSidInDacl(HANDLE object,
                             SecurityObjectType object_type,
                             const base::win::Sid& package_sid,
                             ACCESS_MASK access) {
  if (!AddKnownSidToObject(object, object_type, package_sid,
                           SecurityAccessMode::kRevoke, 0)) {
    return false;
  }

  return AddKnownSidToObject(object, object_type,
                             base::win::WellKnownSid::kAllApplicationPackages,
                             SecurityAccessMode::kGrant, access);
}

absl::optional<base::win::Sid> GetIntegrityLevelSid(
    IntegrityLevel integrity_level) {
  absl::optional<DWORD> value = GetIntegrityLevelValue(integrity_level);
  if (!value)
    return absl::nullopt;
  return base::win::Sid::FromIntegrityLevel(*value);
}

DWORD SetObjectIntegrityLabel(HANDLE handle,
                              SecurityObjectType object_type,
                              DWORD mandatory_policy,
                              IntegrityLevel integrity_level) {
  absl::optional<base::win::Sid> sid = GetIntegrityLevelSid(integrity_level);
  if (!sid)
    return ERROR_INVALID_SID;

  // Get total ACL length. SYSTEM_MANDATORY_LABEL_ACE contains the first DWORD
  // of the SID so remove it from total.
  DWORD length = sizeof(ACL) + sizeof(SYSTEM_MANDATORY_LABEL_ACE) +
                 ::GetLengthSid(sid->GetPSID()) - sizeof(DWORD);
  std::vector<char> sacl_buffer(length);
  PACL sacl = reinterpret_cast<PACL>(sacl_buffer.data());

  if (!::InitializeAcl(sacl, length, ACL_REVISION))
    return ::GetLastError();

  if (!::AddMandatoryAce(sacl, ACL_REVISION, 0, mandatory_policy,
                         sid->GetPSID())) {
    return ::GetLastError();
  }

  DCHECK(::IsValidAcl(sacl));

  return ::SetSecurityInfo(handle, ConvertObjectType(object_type),
                           LABEL_SECURITY_INFORMATION, nullptr, nullptr,
                           nullptr, sacl);
}

}  // namespace sandbox
