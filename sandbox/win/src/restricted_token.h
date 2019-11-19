// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_RESTRICTED_TOKEN_H_
#define SANDBOX_SRC_RESTRICTED_TOKEN_H_

#include <windows.h>

#include <vector>

#include <string>

#include "base/macros.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/security_level.h"
#include "sandbox/win/src/sid.h"

// Flags present in the Group SID list. These 2 flags are new in Windows Vista
#ifndef SE_GROUP_INTEGRITY
#define SE_GROUP_INTEGRITY (0x00000020L)
#endif
#ifndef SE_GROUP_INTEGRITY_ENABLED
#define SE_GROUP_INTEGRITY_ENABLED (0x00000040L)
#endif

namespace sandbox {

// Handles the creation of a restricted token using the effective token or
// any token handle.
// Sample usage:
//    RestrictedToken restricted_token;
//    DWORD err_code = restricted_token.Init(nullptr);  // Use the current
//                                                   // effective token
//    if (ERROR_SUCCESS != err_code) {
//      // handle error.
//    }
//
//    restricted_token.AddRestrictingSid(ATL::Sids::Users().GetPSID());
//    base::win::ScopedHandle token_handle;
//    err_code = restricted_token.GetRestrictedToken(&token_handle);
//    if (ERROR_SUCCESS != err_code) {
//      // handle error.
//    }
//    [...]
class RestrictedToken {
 public:
  // Init() has to be called before calling any other method in the class.
  RestrictedToken();
  ~RestrictedToken();

  // Initializes the RestrictedToken object with effective_token.
  // If effective_token is nullptr, it initializes the RestrictedToken object
  // with the effective token of the current process.
  DWORD Init(HANDLE effective_token);

  // Creates a restricted token.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD GetRestrictedToken(base::win::ScopedHandle* token) const;

  // Creates a restricted token and uses this new token to create a new token
  // for impersonation. Returns this impersonation token.
  //
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  //
  // The sample usage is the same as the GetRestrictedToken function.
  DWORD GetRestrictedTokenForImpersonation(
      base::win::ScopedHandle* token) const;

  // Lists all sids in the token and mark them as Deny Only except for those
  // present in the exceptions parameter. If there is no exception needed,
  // the caller can pass an empty list or nullptr for the exceptions
  // parameter.
  //
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  //
  // Sample usage:
  //    std::vector<Sid> sid_exceptions;
  //    sid_exceptions.push_back(ATL::Sids::Users().GetPSID());
  //    sid_exceptions.push_back(ATL::Sids::World().GetPSID());
  //    restricted_token.AddAllSidsForDenyOnly(&sid_exceptions);
  // Note: A Sid marked for Deny Only in a token cannot be used to grant
  // access to any resource. It can only be used to deny access.
  DWORD AddAllSidsForDenyOnly(std::vector<Sid>* exceptions);

  // Adds a user or group SID for Deny Only in the restricted token.
  // Parameter: sid is the SID to add in the Deny Only list.
  // The return value is always ERROR_SUCCESS.
  //
  // Sample Usage:
  //    restricted_token.AddSidForDenyOnly(ATL::Sids::Admins().GetPSID());
  DWORD AddSidForDenyOnly(const Sid& sid);

  // Adds the user sid of the token for Deny Only in the restricted token.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD AddUserSidForDenyOnly();

  // Lists all privileges in the token and add them to the list of privileges
  // to remove except for those present in the exceptions parameter. If
  // there is no exception needed, the caller can pass an empty list or nullptr
  // for the exceptions parameter.
  //
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  //
  // Sample usage:
  //    std::vector<std::wstring> privilege_exceptions;
  //    privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
  //    restricted_token.DeleteAllPrivileges(&privilege_exceptions);
  DWORD DeleteAllPrivileges(const std::vector<std::wstring>* exceptions);

  // Adds a privilege to the list of privileges to remove in the restricted
  // token.
  // Parameter: privilege is the privilege name to remove. This is the string
  // representing the privilege. (e.g. "SeChangeNotifyPrivilege").
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  //
  // Sample usage:
  //    restricted_token.DeletePrivilege(SE_LOAD_DRIVER_NAME);
  DWORD DeletePrivilege(const wchar_t* privilege);

  // Adds a SID to the list of restricting sids in the restricted token.
  // Parameter: sid is the sid to add to the list restricting sids.
  // The return value is always ERROR_SUCCESS.
  //
  // Sample usage:
  //    restricted_token.AddRestrictingSid(ATL::Sids::Users().GetPSID());
  // Note: The list of restricting is used to force Windows to perform all
  // access checks twice. The first time using your user SID and your groups,
  // and the second time using your list of restricting sids. The access has
  // to be granted in both places to get access to the resource requested.
  DWORD AddRestrictingSid(const Sid& sid);

  // Adds the logon sid of the token in the list of restricting sids for the
  // restricted token.
  //
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD AddRestrictingSidLogonSession();

  // Adds the owner sid of the token in the list of restricting sids for the
  // restricted token.
  //
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD AddRestrictingSidCurrentUser();

  // Adds all group sids and the user sid to the restricting sids list.
  //
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD AddRestrictingSidAllSids();

  // Sets the token integrity level. This is only valid on Vista. The integrity
  // level cannot be higher than your current integrity level.
  DWORD SetIntegrityLevel(IntegrityLevel integrity_level);

  // Set a flag which indicates the created token should have a locked down
  // default DACL when created.
  void SetLockdownDefaultDacl();

 private:
  // The list of restricting sids in the restricted token.
  std::vector<Sid> sids_to_restrict_;
  // The list of privileges to remove in the restricted token.
  std::vector<LUID> privileges_to_disable_;
  // The list of sids to mark as Deny Only in the restricted token.
  std::vector<Sid> sids_for_deny_only_;
  // The token to restrict. Can only be set in a constructor.
  base::win::ScopedHandle effective_token_;
  // The token integrity level. Only valid on Vista.
  IntegrityLevel integrity_level_;
  // Tells if the object is initialized or not (if Init() has been called)
  bool init_;
  // Lockdown the default DACL when creating new tokens.
  bool lockdown_default_dacl_;

  DISALLOW_COPY_AND_ASSIGN(RestrictedToken);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_RESTRICTED_TOKEN_H_
