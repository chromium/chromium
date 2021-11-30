// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/restricted_token.h"

#include <windows.h>

#include <stddef.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/logging.h"
#include "base/win/access_token.h"
#include "sandbox/win/src/acl.h"

namespace {

LUID ConvertToLuid(const CHROME_LUID& luid) {
  LUID ret;
  memcpy(&ret, &luid, sizeof(luid));
  return ret;
}

CHROME_LUID ConvertToChromeLuid(const LUID& luid) {
  CHROME_LUID ret;
  memcpy(&ret, &luid, sizeof(luid));
  return ret;
}

std::vector<SID_AND_ATTRIBUTES> ConvertToAttributes(
    const std::vector<base::win::Sid>& sids,
    DWORD attributes) {
  std::vector<SID_AND_ATTRIBUTES> ret(sids.size());
  for (size_t i = 0; i < sids.size(); ++i) {
    ret[i].Attributes = attributes;
    ret[i].Sid = sids[i].GetPSID();
  }
  return ret;
}

}  // namespace

namespace sandbox {

RestrictedToken::RestrictedToken()
    : integrity_level_(INTEGRITY_LEVEL_LAST),
      init_(false),
      lockdown_default_dacl_(false) {}

RestrictedToken::~RestrictedToken() {}

DWORD RestrictedToken::Init(const HANDLE effective_token) {
  if (init_)
    return ERROR_ALREADY_INITIALIZED;

  HANDLE temp_token;
  if (effective_token) {
    // We duplicate the handle to be able to use it even if the original handle
    // is closed.
    if (!::DuplicateHandle(::GetCurrentProcess(), effective_token,
                           ::GetCurrentProcess(), &temp_token, 0, false,
                           DUPLICATE_SAME_ACCESS)) {
      return ::GetLastError();
    }
  } else {
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                            &temp_token)) {
      return ::GetLastError();
    }
  }
  effective_token_.Set(temp_token);
  absl::optional<base::win::AccessToken> query_token =
      base::win::AccessToken::FromToken(effective_token_.Get());
  if (!query_token)
    return ERROR_NO_TOKEN;
  query_token_.swap(query_token);

  init_ = true;
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::GetRestrictedToken(
    base::win::ScopedHandle* token) const {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  std::vector<SID_AND_ATTRIBUTES> deny_sids =
      ConvertToAttributes(sids_for_deny_only_, SE_GROUP_USE_FOR_DENY_ONLY);
  std::vector<SID_AND_ATTRIBUTES> restrict_sids =
      ConvertToAttributes(sids_to_restrict_, 0);
  std::vector<LUID_AND_ATTRIBUTES> disable_privs(privileges_to_disable_.size());
  for (size_t i = 0; i < privileges_to_disable_.size(); ++i) {
    disable_privs[i].Attributes = 0;
    disable_privs[i].Luid = ConvertToLuid(privileges_to_disable_[i]);
  }

  bool result = true;
  HANDLE new_token_handle = nullptr;
  if (!deny_sids.empty() || !restrict_sids.empty() || !disable_privs.empty()) {
    result = ::CreateRestrictedToken(
        effective_token_.Get(), 0, static_cast<DWORD>(deny_sids.size()),
        deny_sids.data(), static_cast<DWORD>(disable_privs.size()),
        disable_privs.data(), static_cast<DWORD>(restrict_sids.size()),
        restrict_sids.data(), &new_token_handle);
  } else {
    // Duplicate the token even if it's not modified at this point
    // because any subsequent changes to this token would also affect the
    // current process.
    result = ::DuplicateTokenEx(effective_token_.Get(), TOKEN_ALL_ACCESS,
                                nullptr, SecurityIdentification, TokenPrimary,
                                &new_token_handle);
  }

  if (!result)
    return ::GetLastError();

  base::win::ScopedHandle new_token(new_token_handle);

  if (lockdown_default_dacl_) {
    // Don't add Restricted sid and also remove logon sid access.
    if (!RevokeLogonSidFromDefaultDacl(new_token.Get()))
      return ::GetLastError();
  } else {
    // Modify the default dacl on the token to contain Restricted.
    if (!AddSidToDefaultDacl(new_token.Get(),
                             base::win::WellKnownSid::kRestricted,
                             SecurityAccessMode::kGrant, GENERIC_ALL)) {
      return ::GetLastError();
    }
  }

  for (const auto& default_dacl_sid : sids_for_default_dacl_) {
    if (!AddSidToDefaultDacl(new_token.Get(), std::get<0>(default_dacl_sid),
                             std::get<1>(default_dacl_sid),
                             std::get<2>(default_dacl_sid))) {
      return ::GetLastError();
    }
  }

  // Add user to default dacl.
  if (!AddUserSidToDefaultDacl(new_token.Get(), GENERIC_ALL))
    return ::GetLastError();

  DWORD error = SetTokenIntegrityLevel(new_token.Get(), integrity_level_);
  if (ERROR_SUCCESS != error)
    return error;

  HANDLE token_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), new_token.Get(),
                         ::GetCurrentProcess(), &token_handle, TOKEN_ALL_ACCESS,
                         false,  // Don't inherit.
                         0)) {
    return ::GetLastError();
  }

  token->Set(token_handle);
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::GetRestrictedTokenForImpersonation(
    base::win::ScopedHandle* token) const {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  base::win::ScopedHandle restricted_token;
  DWORD err_code = GetRestrictedToken(&restricted_token);
  if (ERROR_SUCCESS != err_code)
    return err_code;

  HANDLE impersonation_token_handle;
  if (!::DuplicateTokenEx(restricted_token.Get(), TOKEN_ALL_ACCESS, nullptr,
                          SecurityImpersonation, TokenImpersonation,
                          &impersonation_token_handle)) {
    return ::GetLastError();
  }
  token->Set(impersonation_token_handle);
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddAllSidsForDenyOnly(
    const std::vector<base::win::Sid>& exceptions) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  // Build the list of the deny only group SIDs
  for (const base::win::AccessToken::Group& group : query_token_->Groups()) {
    if (group.IsIntegrity() || group.IsLogonId())
      continue;
    bool should_ignore = false;
    for (const base::win::Sid& sid : exceptions) {
      if (sid == group.GetSid()) {
        should_ignore = true;
        break;
      }
    }
    if (!should_ignore) {
      sids_for_deny_only_.push_back(group.GetSid().Clone());
    }
  }

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddSidForDenyOnly(const base::win::Sid& sid) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  sids_for_deny_only_.push_back(sid.Clone());
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddSidForDenyOnly(base::win::WellKnownSid known_sid) {
  absl::optional<base::win::Sid> sid = base::win::Sid::FromKnownSid(known_sid);
  if (!sid)
    return ERROR_INVALID_SID;
  return AddSidForDenyOnly(*sid);
}

DWORD RestrictedToken::AddUserSidForDenyOnly() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  sids_for_deny_only_.push_back(query_token_->User());
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::DeleteAllPrivileges(
    const std::vector<std::wstring>& exceptions) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;
  std::unordered_set<std::wstring> privilege_set(exceptions.begin(),
                                                 exceptions.end());
  // Build the list of privileges to disable
  for (const base::win::AccessToken::Privilege& privilege :
       query_token_->Privileges()) {
    if (privilege_set.count(privilege.GetName()) == 0) {
      privileges_to_disable_.push_back(privilege.GetLuid());
    }
  }

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::DeletePrivilege(const wchar_t* privilege) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  LUID luid = {0};
  if (::LookupPrivilegeValue(nullptr, privilege, &luid))
    privileges_to_disable_.push_back(ConvertToChromeLuid(luid));
  else
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddRestrictingSid(const base::win::Sid& sid) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  sids_to_restrict_.push_back(sid.Clone());  // No attributes
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddRestrictingSid(base::win::WellKnownSid known_sid) {
  absl::optional<base::win::Sid> sid = base::win::Sid::FromKnownSid(known_sid);
  if (!sid)
    return ERROR_INVALID_SID;
  return AddRestrictingSid(*sid);
}

DWORD RestrictedToken::AddRestrictingSidLogonSession() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  absl::optional<base::win::Sid> logon_sid = query_token_->LogonId();
  if (logon_sid)
    sids_to_restrict_.push_back(std::move(*logon_sid));
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddRestrictingSidCurrentUser() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;
  sids_to_restrict_.push_back(query_token_->User());
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddRestrictingSidAllSids() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  // Add the current user to the list.
  DWORD error = AddRestrictingSidCurrentUser();
  if (ERROR_SUCCESS != error)
    return error;

  for (const base::win::AccessToken::Group& group : query_token_->Groups()) {
    if (group.IsIntegrity())
      continue;
    AddRestrictingSid(group.GetSid());
  }

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::SetIntegrityLevel(IntegrityLevel integrity_level) {
  integrity_level_ = integrity_level;
  return ERROR_SUCCESS;
}

void RestrictedToken::SetLockdownDefaultDacl() {
  lockdown_default_dacl_ = true;
}

DWORD RestrictedToken::AddDefaultDaclSid(const base::win::Sid& sid,
                                         SecurityAccessMode access_mode,
                                         ACCESS_MASK access) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  sids_for_default_dacl_.push_back(
      std::make_tuple(sid.Clone(), access_mode, access));
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddDefaultDaclSid(base::win::WellKnownSid known_sid,
                                         SecurityAccessMode access_mode,
                                         ACCESS_MASK access) {
  absl::optional<base::win::Sid> sid = base::win::Sid::FromKnownSid(known_sid);
  if (!sid)
    return ERROR_INVALID_SID;
  return AddDefaultDaclSid(*sid, access_mode, access);
}

}  // namespace sandbox
