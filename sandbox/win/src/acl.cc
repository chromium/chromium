// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/acl.h"

#include <aclapi.h>
#include <sddl.h>

#include "base/check.h"
#include "base/memory/free_deleter.h"
#include "base/notreached.h"
#include "base/win/scoped_localalloc.h"

namespace sandbox {

bool GetDefaultDacl(
    HANDLE token,
    std::unique_ptr<TOKEN_DEFAULT_DACL, base::FreeDeleter>* default_dacl) {
  if (!token)
    return false;

  DCHECK(default_dacl);

  unsigned long length = 0;
  ::GetTokenInformation(token, TokenDefaultDacl, nullptr, 0, &length);
  if (length == 0) {
    NOTREACHED();
    return false;
  }

  TOKEN_DEFAULT_DACL* acl =
      reinterpret_cast<TOKEN_DEFAULT_DACL*>(malloc(length));
  default_dacl->reset(acl);

  return !!::GetTokenInformation(token, TokenDefaultDacl, default_dacl->get(),
                                 length, &length);
}

bool AddSidToDacl(const base::win::Sid& sid,
                  ACL* old_dacl,
                  ACCESS_MODE access_mode,
                  ACCESS_MASK access,
                  ACL** new_dacl) {
  EXPLICIT_ACCESS new_access = {0};
  new_access.grfAccessMode = access_mode;
  new_access.grfAccessPermissions = access;
  new_access.grfInheritance = NO_INHERITANCE;

  new_access.Trustee.pMultipleTrustee = nullptr;
  new_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
  new_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  new_access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid.GetPSID());

  if (ERROR_SUCCESS != ::SetEntriesInAcl(1, &new_access, old_dacl, new_dacl))
    return false;

  return true;
}

bool AddSidToDefaultDacl(HANDLE token,
                         const base::win::Sid& sid,
                         ACCESS_MODE access_mode,
                         ACCESS_MASK access) {
  if (!token)
    return false;

  std::unique_ptr<TOKEN_DEFAULT_DACL, base::FreeDeleter> default_dacl;
  if (!GetDefaultDacl(token, &default_dacl))
    return false;

  ACL* new_dacl_ptr = nullptr;
  if (!AddSidToDacl(sid, default_dacl->DefaultDacl, access_mode, access,
                    &new_dacl_ptr)) {
    return false;
  }

  auto new_dacl = base::win::TakeLocalAlloc(new_dacl_ptr);
  TOKEN_DEFAULT_DACL new_token_dacl = {0};
  new_token_dacl.DefaultDacl = new_dacl.get();

  return !!::SetTokenInformation(token, TokenDefaultDacl, &new_token_dacl,
                                 sizeof(new_token_dacl));
}

bool AddSidToDefaultDacl(HANDLE token,
                         base::win::WellKnownSid known_sid,
                         ACCESS_MODE access_mode,
                         ACCESS_MASK access) {
  absl::optional<base::win::Sid> sid = base::win::Sid::FromKnownSid(known_sid);
  if (!sid)
    return false;
  return AddSidToDefaultDacl(token, *sid, access_mode, access);
}

bool RevokeLogonSidFromDefaultDacl(HANDLE token) {
  char logon_sid_buffer[sizeof(TOKEN_GROUPS) + SECURITY_MAX_SID_SIZE];
  DWORD size = sizeof(logon_sid_buffer);

  if (!::GetTokenInformation(token, TokenLogonSid, logon_sid_buffer, size,
                             &size)) {
    // If no logon sid, there's nothing to revoke.
    if (::GetLastError() == ERROR_NOT_FOUND)
      return true;
    return false;
  }
  TOKEN_GROUPS* logon_sid_ptr =
      reinterpret_cast<TOKEN_GROUPS*>(logon_sid_buffer);
  if (logon_sid_ptr->GroupCount < 1) {
    ::SetLastError(ERROR_INVALID_TOKEN);
    return false;
  }
  absl::optional<base::win::Sid> logon_sid =
      base::win::Sid::FromPSID(logon_sid_ptr->Groups[0].Sid);
  if (!logon_sid) {
    ::SetLastError(ERROR_INVALID_SID);
    return false;
  }
  return AddSidToDefaultDacl(token, *logon_sid, REVOKE_ACCESS, 0);
}

bool AddUserSidToDefaultDacl(HANDLE token, ACCESS_MASK access) {
  absl::optional<base::win::Sid> user_sid = base::win::Sid::FromToken(token);
  if (!user_sid)
    return false;

  return AddSidToDefaultDacl(token, *user_sid, GRANT_ACCESS, access);
}

bool AddKnownSidToObject(HANDLE object,
                         SE_OBJECT_TYPE object_type,
                         const base::win::Sid& sid,
                         ACCESS_MODE access_mode,
                         ACCESS_MASK access) {
  PSECURITY_DESCRIPTOR descriptor_ptr = nullptr;
  PACL old_dacl = nullptr;

  if (ERROR_SUCCESS !=
      ::GetSecurityInfo(object, object_type, DACL_SECURITY_INFORMATION, nullptr,
                        nullptr, &old_dacl, nullptr, &descriptor_ptr)) {
    return false;
  }

  auto descriptor = base::win::TakeLocalAlloc(descriptor_ptr);
  PACL new_dacl_ptr = nullptr;
  if (!AddSidToDacl(sid, old_dacl, access_mode, access, &new_dacl_ptr))
    return false;

  auto new_dacl = base::win::TakeLocalAlloc(new_dacl_ptr);
  return ::SetSecurityInfo(object, object_type, DACL_SECURITY_INFORMATION,
                           nullptr, nullptr, new_dacl.get(),
                           nullptr) == ERROR_SUCCESS;
}

bool AddKnownSidToObject(HANDLE object,
                         SE_OBJECT_TYPE object_type,
                         base::win::WellKnownSid known_sid,
                         ACCESS_MODE access_mode,
                         ACCESS_MASK access) {
  absl::optional<base::win::Sid> sid = base::win::Sid::FromKnownSid(known_sid);
  if (!sid)
    return false;
  return AddKnownSidToObject(object, object_type, *sid, access_mode, access);
}

bool ReplacePackageSidInDacl(HANDLE object,
                             SE_OBJECT_TYPE object_type,
                             const base::win::Sid& package_sid,
                             ACCESS_MASK access) {
  if (!AddKnownSidToObject(object, object_type, package_sid, REVOKE_ACCESS,
                           0)) {
    return false;
  }

  return AddKnownSidToObject(
      object, object_type,
      *base::win::Sid::FromKnownSid(
          base::win::WellKnownSid::kAllApplicationPackages),
      GRANT_ACCESS, access);
}

}  // namespace sandbox
