// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/restricted_token_utils.h"

#include <memory>
#include <vector>

#include <optional>
#include "base/check.h"
#include "base/notreached.h"
#include "base/win/access_token.h"
#include "base/win/security_descriptor.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/restricted_token.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/security_level.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

void AddSidException(std::vector<base::win::Sid>& sids,
                     base::win::WellKnownSid known_sid) {
  sids.push_back(base::win::Sid::FromKnownSid(known_sid));
}

}  // namespace

std::optional<base::win::AccessToken> CreateRestrictedToken(
    TokenLevel security_level,
    IntegrityLevel integrity_level,
    TokenType token_type,
    bool lockdown_default_dacl,
    const std::optional<base::win::Sid>& unique_restricted_sid) {
  RestrictedToken restricted_token;
  if (lockdown_default_dacl) {
    restricted_token.SetLockdownDefaultDacl();
  }
  if (unique_restricted_sid) {
    restricted_token.AddDefaultDaclSid(*unique_restricted_sid,
                                       base::win::SecurityAccessMode::kGrant,
                                       GENERIC_ALL);
    restricted_token.AddDefaultDaclSid(
        base::win::WellKnownSid::kCreatorOwnerRights,
        base::win::SecurityAccessMode::kGrant, READ_CONTROL);
  }

  std::vector<std::wstring> privilege_exceptions;
  std::vector<base::win::Sid> sid_exceptions;

  bool deny_sids = true;
  bool remove_privileges = true;
  bool remove_traverse_privilege = false;

  switch (security_level) {
    case USER_UNPROTECTED:
      deny_sids = false;
      remove_privileges = false;
      break;
    case USER_RESTRICTED_SAME_ACCESS:
      deny_sids = false;
      remove_privileges = false;
      restricted_token.AddRestrictingSidAllSids();
      break;
    case USER_RESTRICTED_NON_ADMIN:
      AddSidException(sid_exceptions, base::win::WellKnownSid::kBuiltinUsers);
      AddSidException(sid_exceptions, base::win::WellKnownSid::kWorld);
      AddSidException(sid_exceptions, base::win::WellKnownSid::kInteractive);
      AddSidException(sid_exceptions,
                      base::win::WellKnownSid::kAuthenticatedUser);
      restricted_token.AddRestrictingSid(
          base::win::WellKnownSid::kBuiltinUsers);
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kWorld);
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kInteractive);
      restricted_token.AddRestrictingSid(
          base::win::WellKnownSid::kAuthenticatedUser);
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kRestricted);
      restricted_token.AddRestrictingSidCurrentUser();
      restricted_token.AddRestrictingSidLogonSession();
      if (unique_restricted_sid) {
        restricted_token.AddRestrictingSid(*unique_restricted_sid);
      }
      break;
    case USER_INTERACTIVE:
      AddSidException(sid_exceptions, base::win::WellKnownSid::kBuiltinUsers);
      AddSidException(sid_exceptions, base::win::WellKnownSid::kWorld);
      AddSidException(sid_exceptions, base::win::WellKnownSid::kInteractive);
      AddSidException(sid_exceptions,
                      base::win::WellKnownSid::kAuthenticatedUser);
      restricted_token.AddRestrictingSid(
          base::win::WellKnownSid::kBuiltinUsers);
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kWorld);
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kRestricted);
      restricted_token.AddRestrictingSidCurrentUser();
      restricted_token.AddRestrictingSidLogonSession();
      if (unique_restricted_sid) {
        restricted_token.AddRestrictingSid(*unique_restricted_sid);
      }
      break;
    case USER_LIMITED:
      AddSidException(sid_exceptions, base::win::WellKnownSid::kBuiltinUsers);
      AddSidException(sid_exceptions, base::win::WellKnownSid::kWorld);
      AddSidException(sid_exceptions, base::win::WellKnownSid::kInteractive);
      restricted_token.AddRestrictingSid(
          base::win::WellKnownSid::kBuiltinUsers);
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kWorld);
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kRestricted);
      if (unique_restricted_sid) {
        restricted_token.AddRestrictingSid(*unique_restricted_sid);
      }
      // This token has to be able to create objects in BNO, it needs the
      // current logon sid in the token to achieve this. You should also set the
      // process to be low integrity level so it can't access object created by
      // other processes.
      restricted_token.AddRestrictingSidLogonSession();
      break;
    case USER_LOCKDOWN:
      remove_traverse_privilege = true;
      restricted_token.AddUserSidForDenyOnly();
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kNull);
      if (unique_restricted_sid) {
        restricted_token.AddRestrictingSid(*unique_restricted_sid);
      }
      break;
    case USER_LAST:
      return std::nullopt;
  }

  if (deny_sids) {
    restricted_token.AddAllSidsForDenyOnly(sid_exceptions);
  }

  if (remove_privileges) {
    restricted_token.DeleteAllPrivileges(remove_traverse_privilege);
  }

  restricted_token.SetIntegrityLevel(integrity_level);
  std::optional<base::win::AccessToken> result =
      restricted_token.GetRestrictedToken();
  if (!result) {
    return std::nullopt;
  }

  if (token_type == TokenType::kPrimary) {
    return result;
  }

  result = result->DuplicateImpersonation(
      base::win::SecurityImpersonationLevel::kImpersonation, TOKEN_ALL_ACCESS);
  if (!result) {
    return std::nullopt;
  }

  return result;
}

DWORD HardenTokenIntegrityLevelPolicy(const base::win::AccessToken& token) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          token.get(), base::win::SecurityObjectType::kKernel,
          LABEL_SECURITY_INFORMATION);
  if (!sd) {
    return ::GetLastError();
  }

  // If no SACL then nothing to do.
  if (!sd->sacl()) {
    return ERROR_SUCCESS;
  }
  PACL sacl = sd->sacl()->get();

  for (DWORD ace_index = 0; ace_index < sacl->AceCount; ++ace_index) {
    PSYSTEM_MANDATORY_LABEL_ACE ace;

    if (::GetAce(sacl, ace_index, reinterpret_cast<LPVOID*>(&ace)) &&
        ace->Header.AceType == SYSTEM_MANDATORY_LABEL_ACE_TYPE) {
      ace->Mask |= SYSTEM_MANDATORY_LABEL_NO_READ_UP |
                   SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP;
      break;
    }
  }
  if (!sd->WriteToHandle(token.get(), base::win::SecurityObjectType::kKernel,
                         LABEL_SECURITY_INFORMATION)) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

}  // namespace sandbox
