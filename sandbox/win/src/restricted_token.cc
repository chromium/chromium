// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/restricted_token.h"

#include <windows.h>

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/logging.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/win_utils.h"

namespace {

// Wrapper for utility version to unwrap ScopedHandle.
std::unique_ptr<BYTE[]> GetTokenInfo(const base::win::ScopedHandle& token,
                                     TOKEN_INFORMATION_CLASS info_class,
                                     DWORD* error) {
  std::unique_ptr<BYTE[]> buffer;
  *error = sandbox::GetTokenInformation(token.Get(), info_class, &buffer);
  if (*error != ERROR_SUCCESS)
    return nullptr;
  return buffer;
}

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
  if (!::DuplicateToken(restricted_token.Get(), SecurityImpersonation,
                        &impersonation_token_handle)) {
    return ::GetLastError();
  }
  base::win::ScopedHandle impersonation_token(impersonation_token_handle);

  HANDLE token_handle;
  if (!::DuplicateHandle(::GetCurrentProcess(), impersonation_token.Get(),
                         ::GetCurrentProcess(), &token_handle, TOKEN_ALL_ACCESS,
                         false,  // Don't inherit.
                         0)) {
    return ::GetLastError();
  }

  token->Set(token_handle);
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddAllSidsForDenyOnly(
    const std::vector<base::win::Sid>& exceptions) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  DWORD error;
  std::unique_ptr<BYTE[]> buffer =
      GetTokenInfo(effective_token_, TokenGroups, &error);

  if (!buffer)
    return error;

  TOKEN_GROUPS* token_groups = reinterpret_cast<TOKEN_GROUPS*>(buffer.get());

  // Build the list of the deny only group SIDs
  for (unsigned int i = 0; i < token_groups->GroupCount; ++i) {
    if ((token_groups->Groups[i].Attributes & SE_GROUP_INTEGRITY) == 0 &&
        (token_groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) == 0) {
      bool should_ignore = false;
      for (const base::win::Sid& sid : exceptions) {
        if (sid.Equal(token_groups->Groups[i].Sid)) {
          should_ignore = true;
          break;
        }
      }
      if (!should_ignore) {
        sids_for_deny_only_.push_back(
            *base::win::Sid::FromPSID(token_groups->Groups[i].Sid));
      }
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

  absl::optional<base::win::Sid> user = base::win::Sid::CurrentUser();
  if (!user)
    return ERROR_INVALID_SID;

  sids_for_deny_only_.push_back(std::move(*user));

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::DeleteAllPrivileges(
    const std::vector<std::wstring>& exceptions) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  DWORD error;
  std::unique_ptr<BYTE[]> buffer =
      GetTokenInfo(effective_token_, TokenPrivileges, &error);

  if (!buffer)
    return error;

  TOKEN_PRIVILEGES* token_privileges =
      reinterpret_cast<TOKEN_PRIVILEGES*>(buffer.get());

  // Build the list of privileges to disable
  for (unsigned int i = 0; i < token_privileges->PrivilegeCount; ++i) {
    bool should_ignore = false;
    for (const std::wstring& name : exceptions) {
      LUID luid = {};
      ::LookupPrivilegeValue(nullptr, name.c_str(), &luid);
      if (token_privileges->Privileges[i].Luid.HighPart == luid.HighPart &&
          token_privileges->Privileges[i].Luid.LowPart == luid.LowPart) {
        should_ignore = true;
        break;
      }
    }
    if (!should_ignore) {
      privileges_to_disable_.push_back(
          ConvertToChromeLuid(token_privileges->Privileges[i].Luid));
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

  DWORD error;
  std::unique_ptr<BYTE[]> buffer =
      GetTokenInfo(effective_token_, TokenGroups, &error);

  if (!buffer)
    return error;

  TOKEN_GROUPS* token_groups = reinterpret_cast<TOKEN_GROUPS*>(buffer.get());

  PSID logon_sid_ptr = nullptr;
  for (unsigned int i = 0; i < token_groups->GroupCount; ++i) {
    if ((token_groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) != 0) {
      logon_sid_ptr = token_groups->Groups[i].Sid;
      break;
    }
  }

  if (logon_sid_ptr) {
    absl::optional<base::win::Sid> logon_sid =
        base::win::Sid::FromPSID(logon_sid_ptr);
    if (!logon_sid)
      return ERROR_INVALID_SID;
    sids_to_restrict_.push_back(std::move(*logon_sid));
  }
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddRestrictingSidCurrentUser() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  absl::optional<base::win::Sid> user = base::win::Sid::CurrentUser();
  if (!user)
    return ERROR_INVALID_SID;
  sids_to_restrict_.push_back(std::move(*user));

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

  std::unique_ptr<BYTE[]> buffer =
      GetTokenInfo(effective_token_, TokenGroups, &error);

  if (!buffer)
    return error;

  TOKEN_GROUPS* token_groups = reinterpret_cast<TOKEN_GROUPS*>(buffer.get());

  // Build the list of restricting sids from all groups.
  for (unsigned int i = 0; i < token_groups->GroupCount; ++i) {
    if ((token_groups->Groups[i].Attributes & SE_GROUP_INTEGRITY) == 0) {
      absl::optional<base::win::Sid> sid =
          base::win::Sid::FromPSID(token_groups->Groups[i].Sid);
      if (!sid)
        return ERROR_INVALID_SID;
      AddRestrictingSid(*sid);
    }
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
