// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/acl.h"

#include <aclapi.h>
#include <sddl.h>

#include "base/check.h"
#include "base/memory/free_deleter.h"
#include "base/notreached.h"

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

  if (!::GetTokenInformation(token, TokenDefaultDacl, default_dacl->get(),
                             length, &length))
    return false;

  return true;
}

bool AddSidToDacl(const Sid& sid,
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
                         const Sid& sid,
                         ACCESS_MODE access_mode,
                         ACCESS_MASK access) {
  if (!token)
    return false;

  std::unique_ptr<TOKEN_DEFAULT_DACL, base::FreeDeleter> default_dacl;
  if (!GetDefaultDacl(token, &default_dacl))
    return false;

  ACL* new_dacl = nullptr;
  if (!AddSidToDacl(sid, default_dacl->DefaultDacl, access_mode, access,
                    &new_dacl))
    return false;

  TOKEN_DEFAULT_DACL new_token_dacl = {0};
  new_token_dacl.DefaultDacl = new_dacl;

  bool ret = ::SetTokenInformation(token, TokenDefaultDacl, &new_token_dacl,
                                   sizeof(new_token_dacl));
  ::LocalFree(new_dacl);
  return ret;
}

bool RevokeLogonSidFromDefaultDacl(HANDLE token) {
  DWORD size = sizeof(TOKEN_GROUPS) + SECURITY_MAX_SID_SIZE;
  TOKEN_GROUPS* logon_sid = reinterpret_cast<TOKEN_GROUPS*>(malloc(size));

  std::unique_ptr<TOKEN_GROUPS, base::FreeDeleter> logon_sid_ptr(logon_sid);

  if (!::GetTokenInformation(token, TokenLogonSid, logon_sid, size, &size)) {
    // If no logon sid, there's nothing to revoke.
    if (::GetLastError() == ERROR_NOT_FOUND)
      return true;
    return false;
  }
  if (logon_sid->GroupCount < 1) {
    ::SetLastError(ERROR_INVALID_TOKEN);
    return false;
  }
  return AddSidToDefaultDacl(token,
                             reinterpret_cast<SID*>(logon_sid->Groups[0].Sid),
                             REVOKE_ACCESS, 0);
}

bool AddUserSidToDefaultDacl(HANDLE token, ACCESS_MASK access) {
  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  TOKEN_USER* token_user = reinterpret_cast<TOKEN_USER*>(malloc(size));

  std::unique_ptr<TOKEN_USER, base::FreeDeleter> token_user_ptr(token_user);

  if (!::GetTokenInformation(token, TokenUser, token_user, size, &size))
    return false;

  return AddSidToDefaultDacl(token,
                             reinterpret_cast<SID*>(token_user->User.Sid),
                             GRANT_ACCESS, access);
}

bool AddKnownSidToObject(HANDLE object,
                         SE_OBJECT_TYPE object_type,
                         const Sid& sid,
                         ACCESS_MODE access_mode,
                         ACCESS_MASK access) {
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  PACL old_dacl = nullptr;
  PACL new_dacl = nullptr;

  if (ERROR_SUCCESS !=
      ::GetSecurityInfo(object, object_type, DACL_SECURITY_INFORMATION, nullptr,
                        nullptr, &old_dacl, nullptr, &descriptor))
    return false;

  if (!AddSidToDacl(sid, old_dacl, access_mode, access, &new_dacl)) {
    ::LocalFree(descriptor);
    return false;
  }

  DWORD result =
      ::SetSecurityInfo(object, object_type, DACL_SECURITY_INFORMATION, nullptr,
                        nullptr, new_dacl, nullptr);

  ::LocalFree(new_dacl);
  ::LocalFree(descriptor);

  if (ERROR_SUCCESS != result)
    return false;

  return true;
}

bool ReplacePackageSidInDacl(HANDLE object,
                             SE_OBJECT_TYPE object_type,
                             const Sid& package_sid,
                             ACCESS_MASK access) {
  if (!AddKnownSidToObject(object, object_type, package_sid, REVOKE_ACCESS,
                           0)) {
    return false;
  }

  Sid any_package_sid(::WinBuiltinAnyPackageSid);
  if (!AddKnownSidToObject(object, object_type, any_package_sid, GRANT_ACCESS,
                           access)) {
    return false;
  }
  return true;
}

}  // namespace sandbox
