// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_RESTRICTED_TOKEN_H_
#define SANDBOX_WIN_SRC_RESTRICTED_TOKEN_H_

#include <vector>

#include <string>
#include <tuple>

#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/security_level.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  RestrictedToken(const RestrictedToken&) = delete;
  RestrictedToken& operator=(const RestrictedToken&) = delete;

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
  //    std::vector<base::win::Sid> sid_exceptions;
  //    sid_exceptions.push_back(*base::win::Sid::FromPSID(psid));
  //    restricted_token.AddAllSidsForDenyOnly(sid_exceptions);
  // Note: A Sid marked for Deny Only in a token cannot be used to grant
  // access to any resource. It can only be used to deny access.
  DWORD AddAllSidsForDenyOnly(const std::vector<base::win::Sid>& exceptions);

  // Adds a user or group SID for Deny Only in the restricted token.
  // Parameter: sid is the SID to add in the Deny Only list.
  // The return value is always ERROR_SUCCESS.
  //
  // Sample Usage:
  //    restricted_token.AddSidForDenyOnly(sid);
  DWORD AddSidForDenyOnly(const base::win::Sid& sid);

  // Adds a known SID for Deny Only in the restricted token.
  // Parameter: known_sid is the SID to add in the Deny Only list.
  // The return value is always ERROR_SUCCESS.
  //
  // Sample Usage:
  //    restricted_token.AddSidForDenyOnly(base::win::WellKnownSid::kWorld);
  DWORD AddSidForDenyOnly(base::win::WellKnownSid known_sid);

  // Adds the user sid of the token for Deny Only in the restricted token.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD AddUserSidForDenyOnly();

  // Specify to remove all privileges in the restricted token. By default this
  // will not remove SeChangeNotifyPrivilege, however you can specify true for
  // |remove_traversal_privilege| to remove that privilege as well.
  //
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD DeleteAllPrivileges(bool remove_traversal_privilege);

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
  DWORD AddRestrictingSid(const base::win::Sid& sid);

  // Adds a known SID to the list of restricting sids in the restricted token.
  // Parameter: known_sid is the sid to add to the list restricting sids.
  // The return value is always ERROR_SUCCESS.
  //
  // Sample usage:
  //    restricted_token.AddRestrictingSid(base::win::WellKnownSid::kWorld);
  // Note: The list of restricting is used to force Windows to perform all
  // access checks twice. The first time using your user SID and your groups,
  // and the second time using your list of restricting sids. The access has
  // to be granted in both places to get access to the resource requested.
  DWORD AddRestrictingSid(base::win::WellKnownSid known_sid);

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

  // Sets the token integrity level. The integrity level cannot be higher than
  // your current integrity level.
  DWORD SetIntegrityLevel(IntegrityLevel integrity_level);

  // Set a flag which indicates the created token should have a locked down
  // default DACL when created.
  void SetLockdownDefaultDacl();

  // Add a SID to the default DACL. These SIDs are added regardless of the
  // SetLockdownDefaultDacl state.
  DWORD AddDefaultDaclSid(const base::win::Sid& sid,
                          SecurityAccessMode access_mode,
                          ACCESS_MASK access);

  // Add a SID to the default DACL. These SIDs are added regardless of the
  // SetLockdownDefaultDacl state.
  DWORD AddDefaultDaclSid(base::win::WellKnownSid known_sid,
                          SecurityAccessMode access_mode,
                          ACCESS_MASK access);

 private:
  // The list of restricting sids in the restricted token.
  std::vector<base::win::Sid> sids_to_restrict_;
  // The list of sids to mark as Deny Only in the restricted token.
  std::vector<base::win::Sid> sids_for_deny_only_;
  // The list of sids to add to the default DACL of the restricted token.
  std::vector<std::tuple<base::win::Sid, SecurityAccessMode, ACCESS_MASK>>
      sids_for_default_dacl_;
  // The token to restrict. Can only be set in a constructor.
  base::win::ScopedHandle effective_token_;
  // The token in a form for querying.
  absl::optional<base::win::AccessToken> query_token_;
  // The token integrity level.
  IntegrityLevel integrity_level_;
  // Tells if the object is initialized or not (if Init() has been called)
  bool init_;
  // Lockdown the default DACL when creating new tokens.
  bool lockdown_default_dacl_;
  // Delete all privileges except for SeChangeNotifyPrivilege.
  bool delete_all_privileges_;
  // Also delete SeChangeNotifyPrivilege if delete_all_privileges_ is true.
  bool remove_traversal_privilege_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_RESTRICTED_TOKEN_H_
