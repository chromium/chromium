// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_RESTRICTED_TOKEN_H_
#define SANDBOX_WIN_SRC_RESTRICTED_TOKEN_H_

#include <vector>

#include <optional>
#include "base/win/access_control_list.h"
#include "base/win/access_token.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/security_level.h"

namespace sandbox {

// Handles the creation of a restricted token using the effective token or
// any token handle.
// Sample usage:
//    RestrictedToken restricted_token;
//
//    restricted_token.AddRestrictingSid(base::win::WellKnownSid::kUsers);
//    auto token = restricted_token.GetRestrictedToken();
//    if (!token) {
//      // handle error.
//    }
//    [...]
class RestrictedToken {
 public:
  RestrictedToken();

  RestrictedToken(const RestrictedToken&) = delete;
  RestrictedToken& operator=(const RestrictedToken&) = delete;

  ~RestrictedToken();

  // Creates a restricted token. This creates a primary token for process
  // creation. If the function fails an empty value is returned.
  std::optional<base::win::AccessToken> GetRestrictedToken() const;

  // Lists all sids in the token and mark them as Deny Only except for those
  // present in the exceptions parameter. If there is no exception needed,
  // the caller can pass an empty list or nullptr for the exceptions
  // parameter.
  //
  // Sample usage:
  //    std::vector<base::win::Sid> sid_exceptions;
  //    sid_exceptions.emplace_back(base::win::WellKnownSid::kWorld);
  //    restricted_token.AddAllSidsForDenyOnly(sid_exceptions);
  // Note: A Sid marked for Deny Only in a token cannot be used to grant
  // access to any resource. It can only be used to deny access.
  void AddAllSidsForDenyOnly(const std::vector<base::win::Sid>& exceptions);

  // Adds a user or group SID for Deny Only in the restricted token.
  // `sid` is the SID to add in the Deny Only list.
  //
  // Sample Usage:
  //    restricted_token.AddSidForDenyOnly(sid);
  void AddSidForDenyOnly(const base::win::Sid& sid);

  // Adds a known SID for Deny Only in the restricted token.
  // `known_sid` is the SID to add in the Deny Only list.
  // Sample Usage:
  //    restricted_token.AddSidForDenyOnly(base::win::WellKnownSid::kWorld);
  void AddSidForDenyOnly(base::win::WellKnownSid known_sid);

  // Adds the user sid of the token for Deny Only in the restricted token.
  void AddUserSidForDenyOnly();

  // Specify to remove all privileges in the restricted token. By default this
  // will not remove SeChangeNotifyPrivilege, however you can specify true for
  // `remove_traversal_privilege` to remove that privilege as well.
  void DeleteAllPrivileges(bool remove_traversal_privilege);

  // Adds a SID to the list of restricting sids in the restricted token.
  // `sid` is the sid to add to the list restricting sids.
  //
  // Sample usage:
  //    restricted_token.AddRestrictingSid(ATL::Sids::Users().GetPSID());
  // Note: The list of restricting is used to force Windows to perform all
  // access checks twice. The first time using your user SID and your groups,
  // and the second time using your list of restricting sids. The access has
  // to be granted in both places to get access to the resource requested.
  void AddRestrictingSid(const base::win::Sid& sid);

  // Adds a known SID to the list of restricting sids in the restricted token.
  // `known_sid` is the sid to add to the list restricting sids.

  // Sample usage:
  //    restricted_token.AddRestrictingSid(base::win::WellKnownSid::kWorld);
  // Note: The list of restricting is used to force Windows to perform all
  // access checks twice. The first time using your user SID and your groups,
  // and the second time using your list of restricting sids. The access has
  // to be granted in both places to get access to the resource requested.
  void AddRestrictingSid(base::win::WellKnownSid known_sid);

  // Adds the logon sid of the token in the list of restricting sids for the
  // restricted token.
  void AddRestrictingSidLogonSession();

  // Adds the owner sid of the token in the list of restricting sids for the
  // restricted token.
  void AddRestrictingSidCurrentUser();

  // Adds all group sids and the user sid to the restricting sids list.
  void AddRestrictingSidAllSids();

  // Sets the token integrity level. The integrity level cannot be higher than
  // your current integrity level.
  void SetIntegrityLevel(IntegrityLevel integrity_level);

  // Set a flag which indicates the created token should have a locked down
  // default DACL when created.
  void SetLockdownDefaultDacl();

  // Add a SID to the default DACL. These SIDs are added regardless of the
  // SetLockdownDefaultDacl state.
  void AddDefaultDaclSid(const base::win::Sid& sid,
                         base::win::SecurityAccessMode access_mode,
                         ACCESS_MASK access);

  // Add a SID to the default DACL. These SIDs are added regardless of the
  // SetLockdownDefaultDacl state.
  void AddDefaultDaclSid(base::win::WellKnownSid known_sid,
                         base::win::SecurityAccessMode access_mode,
                         ACCESS_MASK access);

  // Creates a restricted token. This is only used for testing to change the
  // token used to build the restricted token.
  std::optional<base::win::AccessToken> GetRestrictedTokenForTesting(
      base::win::AccessToken& token);

 private:
  std::vector<base::win::Sid> BuildDenyOnlySids(
      const base::win::AccessToken& token) const;
  std::vector<base::win::Sid> BuildRestrictedSids(
      const base::win::AccessToken& token) const;
  std::optional<base::win::AccessToken> CreateRestricted(
      const base::win::AccessToken& token) const;

  // The list of restricting sids in the restricted token.
  std::vector<base::win::Sid> sids_to_restrict_;
  // The list of sids to mark as Deny Only in the restricted token.
  std::vector<base::win::Sid> sids_for_deny_only_;
  // The list of sids to add to the default DACL of the restricted token.
  std::vector<base::win::ExplicitAccessEntry> sids_for_default_dacl_;
  // The token to restrict, this is only used for testing.
  std::optional<base::win::AccessToken> effective_token_;
  // The token integrity level RID.
  std::optional<DWORD> integrity_rid_;
  // Lockdown the default DACL when creating new tokens.
  bool lockdown_default_dacl_ = false;
  // Delete all privileges except for SeChangeNotifyPrivilege.
  bool delete_all_privileges_ = false;
  // Also delete SeChangeNotifyPrivilege if delete_all_privileges_ is true.
  bool remove_traversal_privilege_ = false;
  // Add all SIDs for deny only.
  bool add_all_sids_for_deny_only_ = false;
  // The list of exceptions when adding all SIDs for deny only.
  std::vector<base::win::Sid> add_all_exceptions_;
  // Add the user's sid to the deny only list.
  bool add_user_sid_for_deny_only_ = false;
  // Add the logon session SID to the restricted SID list.
  bool add_restricting_sid_logon_session_ = false;
  // Add the current user SID to the restricted SID list.
  bool add_restricting_sid_current_user_ = false;
  // Add all SIDs to the restricted SIDs.
  bool add_restricting_sid_all_sids_ = false;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_RESTRICTED_TOKEN_H_
