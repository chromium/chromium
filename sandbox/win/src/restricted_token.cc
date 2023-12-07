// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/restricted_token.h"

#include <windows.h>

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/win/access_token.h"
#include "base/win/security_util.h"
#include "sandbox/win/src/acl.h"

namespace sandbox {

RestrictedToken::RestrictedToken() = default;
RestrictedToken::~RestrictedToken() = default;

std::optional<base::win::AccessToken> RestrictedToken::GetRestrictedToken()
    const {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess(/*impersonation=*/false,
                                                 TOKEN_ALL_ACCESS);
  if (!token) {
    return std::nullopt;
  }
  return CreateRestricted(*token);
}

void RestrictedToken::AddAllSidsForDenyOnly(
    const std::vector<base::win::Sid>& exceptions) {
  add_all_sids_for_deny_only_ = true;
  add_all_exceptions_ = base::win::CloneSidVector(exceptions);
}

void RestrictedToken::AddSidForDenyOnly(const base::win::Sid& sid) {
  sids_for_deny_only_.push_back(sid.Clone());
}

void RestrictedToken::AddSidForDenyOnly(base::win::WellKnownSid known_sid) {
  sids_for_deny_only_.emplace_back(known_sid);
}

void RestrictedToken::AddUserSidForDenyOnly() {
  add_user_sid_for_deny_only_ = true;
}

void RestrictedToken::DeleteAllPrivileges(bool remove_traversal_privilege) {
  delete_all_privileges_ = true;
  remove_traversal_privilege_ = remove_traversal_privilege;
}

void RestrictedToken::AddRestrictingSid(const base::win::Sid& sid) {
  sids_to_restrict_.push_back(sid.Clone());
}

void RestrictedToken::AddRestrictingSid(base::win::WellKnownSid known_sid) {
  sids_to_restrict_.emplace_back(known_sid);
}

void RestrictedToken::AddRestrictingSidLogonSession() {
  add_restricting_sid_logon_session_ = true;
}

void RestrictedToken::AddRestrictingSidCurrentUser() {
  add_restricting_sid_current_user_ = true;
}

void RestrictedToken::AddRestrictingSidAllSids() {
  add_restricting_sid_all_sids_ = true;
  AddRestrictingSidCurrentUser();
}

void RestrictedToken::SetIntegrityLevel(IntegrityLevel integrity_level) {
  integrity_rid_ = GetIntegrityLevelRid(integrity_level);
}

void RestrictedToken::SetLockdownDefaultDacl() {
  lockdown_default_dacl_ = true;
}

void RestrictedToken::AddDefaultDaclSid(
    const base::win::Sid& sid,
    base::win::SecurityAccessMode access_mode,
    ACCESS_MASK access) {
  sids_for_default_dacl_.emplace_back(sid.Clone(), access_mode, access, 0);
}

void RestrictedToken::AddDefaultDaclSid(
    base::win::WellKnownSid known_sid,
    base::win::SecurityAccessMode access_mode,
    ACCESS_MASK access) {
  sids_for_default_dacl_.emplace_back(known_sid, access_mode, access, 0);
}

std::optional<base::win::AccessToken>
RestrictedToken::GetRestrictedTokenForTesting(base::win::AccessToken& token) {
  return CreateRestricted(token);
}

std::vector<base::win::Sid> RestrictedToken::BuildDenyOnlySids(
    const base::win::AccessToken& token) const {
  std::vector<base::win::Sid> sids =
      base::win::CloneSidVector(sids_for_deny_only_);
  if (add_user_sid_for_deny_only_) {
    sids.push_back(token.User());
  }
  if (add_all_sids_for_deny_only_) {
    // Build the list of the deny only group SIDs
    for (const base::win::AccessToken::Group& group : token.Groups()) {
      if (group.IsIntegrity() || group.IsLogonId()) {
        continue;
      }
      if (base::ranges::find(add_all_exceptions_, group.GetSid()) ==
          add_all_exceptions_.end()) {
        sids.push_back(group.GetSid().Clone());
      }
    }
  }
  return sids;
}

std::vector<base::win::Sid> RestrictedToken::BuildRestrictedSids(
    const base::win::AccessToken& token) const {
  std::vector<base::win::Sid> sids =
      base::win::CloneSidVector(sids_to_restrict_);
  if (add_restricting_sid_current_user_) {
    sids.push_back(token.User());
  }
  if (add_restricting_sid_all_sids_) {
    for (const base::win::AccessToken::Group& group : token.Groups()) {
      if (group.IsIntegrity()) {
        continue;
      }
      sids.push_back(group.GetSid().Clone());
    }
  }
  if (add_restricting_sid_logon_session_) {
    std::optional<base::win::Sid> logon_sid = token.LogonId();
    if (logon_sid.has_value()) {
      sids.push_back(std::move(*logon_sid));
    }
  }
  return sids;
}

std::optional<base::win::AccessToken> RestrictedToken::CreateRestricted(
    const base::win::AccessToken& token) const {
  std::optional<base::win::AccessToken> new_token;

  std::vector<base::win::Sid> deny_sids = BuildDenyOnlySids(token);
  std::vector<base::win::Sid> restrict_sids = BuildRestrictedSids(token);
  if (!deny_sids.empty() || !restrict_sids.empty() || delete_all_privileges_) {
    new_token = token.CreateRestricted(
        delete_all_privileges_ ? DISABLE_MAX_PRIVILEGE : 0, deny_sids, {},
        restrict_sids, TOKEN_ALL_ACCESS);
  } else {
    // Duplicate the token even if it's not modified at this point
    // because any subsequent changes to this token would also affect the
    // current process.
    new_token = token.DuplicatePrimary(TOKEN_ALL_ACCESS);
  }

  if (!new_token) {
    return std::nullopt;
  }

  if (delete_all_privileges_ && remove_traversal_privilege_ &&
      !new_token->RemoveAllPrivileges()) {
    return std::nullopt;
  }

  std::vector<base::win::ExplicitAccessEntry> dacl_entries;

  std::optional<base::win::AccessControlList> dacl = new_token->DefaultDacl();
  if (!dacl) {
    return std::nullopt;
  }

  if (lockdown_default_dacl_) {
    // Don't add Restricted sid and also remove logon sid access.
    std::optional<base::win::Sid> logon_sid = new_token->LogonId();
    if (logon_sid.has_value()) {
      dacl_entries.emplace_back(*logon_sid,
                                base::win::SecurityAccessMode::kRevoke, 0, 0);
    } else {
      DWORD last_error = ::GetLastError();
      if (last_error != ERROR_NOT_FOUND) {
        return std::nullopt;
      }
    }
  } else {
    dacl_entries.emplace_back(base::win::WellKnownSid::kRestricted,
                              base::win::SecurityAccessMode::kGrant,
                              GENERIC_ALL, 0);
  }

  for (const base::win::ExplicitAccessEntry& entry : sids_for_default_dacl_) {
    dacl_entries.push_back(entry.Clone());
  }

  dacl_entries.emplace_back(
      new_token->User(), base::win::SecurityAccessMode::kGrant, GENERIC_ALL, 0);

  if (!dacl->SetEntries(dacl_entries)) {
    return std::nullopt;
  }

  if (!new_token->SetDefaultDacl(*dacl)) {
    return std::nullopt;
  }

  if (integrity_rid_.has_value()) {
    if (!new_token->SetIntegrityLevel(*integrity_rid_)) {
      return std::nullopt;
    }
  }

  return new_token;
}

}  // namespace sandbox
