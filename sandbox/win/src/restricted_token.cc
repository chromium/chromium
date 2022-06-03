// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/restricted_token.h"

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

  size_t deny_size = sids_for_deny_only_.size();
  size_t restrict_size = sids_to_restrict_.size();
  size_t privileges_size = privileges_to_disable_.size();

  SID_AND_ATTRIBUTES* deny_only_array = nullptr;
  if (deny_size) {
    deny_only_array = new SID_AND_ATTRIBUTES[deny_size];

    for (unsigned int i = 0; i < sids_for_deny_only_.size(); ++i) {
      deny_only_array[i].Attributes = SE_GROUP_USE_FOR_DENY_ONLY;
      deny_only_array[i].Sid = sids_for_deny_only_[i].GetPSID();
    }
  }

  SID_AND_ATTRIBUTES* sids_to_restrict_array = nullptr;
  if (restrict_size) {
    sids_to_restrict_array = new SID_AND_ATTRIBUTES[restrict_size];

    for (unsigned int i = 0; i < restrict_size; ++i) {
      sids_to_restrict_array[i].Attributes = 0;
      sids_to_restrict_array[i].Sid = sids_to_restrict_[i].GetPSID();
    }
  }

  LUID_AND_ATTRIBUTES* privileges_to_disable_array = nullptr;
  if (privileges_size) {
    privileges_to_disable_array = new LUID_AND_ATTRIBUTES[privileges_size];

    for (unsigned int i = 0; i < privileges_size; ++i) {
      privileges_to_disable_array[i].Attributes = 0;
      privileges_to_disable_array[i].Luid = privileges_to_disable_[i];
    }
  }

  bool result = true;
  HANDLE new_token_handle = nullptr;
  if (deny_size || restrict_size || privileges_size) {
    result = ::CreateRestrictedToken(
        effective_token_.Get(), 0, static_cast<DWORD>(deny_size),
        deny_only_array, static_cast<DWORD>(privileges_size),
        privileges_to_disable_array, static_cast<DWORD>(restrict_size),
        sids_to_restrict_array, &new_token_handle);
  } else {
    // Duplicate the token even if it's not modified at this point
    // because any subsequent changes to this token would also affect the
    // current process.
    result = ::DuplicateTokenEx(effective_token_.Get(), TOKEN_ALL_ACCESS,
                                nullptr, SecurityIdentification, TokenPrimary,
                                &new_token_handle);
  }
  auto last_error = ::GetLastError();

  if (deny_only_array)
    delete[] deny_only_array;

  if (sids_to_restrict_array)
    delete[] sids_to_restrict_array;

  if (privileges_to_disable_array)
    delete[] privileges_to_disable_array;

  if (!result)
    return last_error;

  base::win::ScopedHandle new_token(new_token_handle);

  if (lockdown_default_dacl_) {
    // Don't add Restricted sid and also remove logon sid access.
    if (!RevokeLogonSidFromDefaultDacl(new_token.Get()))
      return ::GetLastError();
  } else {
    // Modify the default dacl on the token to contain Restricted.
    if (!AddSidToDefaultDacl(new_token.Get(), WinRestrictedCodeSid,
                             GRANT_ACCESS, GENERIC_ALL)) {
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

DWORD RestrictedToken::AddAllSidsForDenyOnly(std::vector<Sid>* exceptions) {
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
      if (exceptions) {
        for (unsigned int j = 0; j < exceptions->size(); ++j) {
          if (::EqualSid((*exceptions)[j].GetPSID(),
                         token_groups->Groups[i].Sid)) {
            should_ignore = true;
            break;
          }
        }
      }
      if (!should_ignore) {
        sids_for_deny_only_.push_back(
            reinterpret_cast<SID*>(token_groups->Groups[i].Sid));
      }
    }
  }

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddSidForDenyOnly(const Sid& sid) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  sids_for_deny_only_.push_back(sid);
  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddUserSidForDenyOnly() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  std::unique_ptr<BYTE[]> buffer(new BYTE[size]);
  TOKEN_USER* token_user = reinterpret_cast<TOKEN_USER*>(buffer.get());

  bool result = ::GetTokenInformation(effective_token_.Get(), TokenUser,
                                      token_user, size, &size);

  if (!result)
    return ::GetLastError();

  Sid user = reinterpret_cast<SID*>(token_user->User.Sid);
  sids_for_deny_only_.push_back(user);

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::DeleteAllPrivileges(
    const std::vector<std::wstring>* exceptions) {
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
    if (exceptions) {
      for (unsigned int j = 0; j < exceptions->size(); ++j) {
        LUID luid = {0};
        ::LookupPrivilegeValue(nullptr, (*exceptions)[j].c_str(), &luid);
        if (token_privileges->Privileges[i].Luid.HighPart == luid.HighPart &&
            token_privileges->Privileges[i].Luid.LowPart == luid.LowPart) {
          should_ignore = true;
          break;
        }
      }
    }
    if (!should_ignore) {
      privileges_to_disable_.push_back(token_privileges->Privileges[i].Luid);
    }
  }

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::DeletePrivilege(const wchar_t* privilege) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  LUID luid = {0};
  if (LookupPrivilegeValue(nullptr, privilege, &luid))
    privileges_to_disable_.push_back(luid);
  else
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddRestrictingSid(const Sid& sid) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  sids_to_restrict_.push_back(sid);  // No attributes
  return ERROR_SUCCESS;
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

  SID* logon_sid = nullptr;
  for (unsigned int i = 0; i < token_groups->GroupCount; ++i) {
    if ((token_groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) != 0) {
      logon_sid = static_cast<SID*>(token_groups->Groups[i].Sid);
      break;
    }
  }

  if (logon_sid)
    sids_to_restrict_.push_back(logon_sid);

  return ERROR_SUCCESS;
}

DWORD RestrictedToken::AddRestrictingSidCurrentUser() {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  std::unique_ptr<BYTE[]> buffer(new BYTE[size]);
  TOKEN_USER* token_user = reinterpret_cast<TOKEN_USER*>(buffer.get());

  bool result = ::GetTokenInformation(effective_token_.Get(), TokenUser,
                                      token_user, size, &size);

  if (!result)
    return ::GetLastError();

  Sid user = reinterpret_cast<SID*>(token_user->User.Sid);
  sids_to_restrict_.push_back(user);

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
    if ((token_groups->Groups[i].Attributes & SE_GROUP_INTEGRITY) == 0)
      AddRestrictingSid(reinterpret_cast<SID*>(token_groups->Groups[i].Sid));
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

DWORD RestrictedToken::AddDefaultDaclSid(const Sid& sid,
                                         ACCESS_MODE access_mode,
                                         ACCESS_MASK access) {
  DCHECK(init_);
  if (!init_)
    return ERROR_NO_TOKEN;

  sids_for_default_dacl_.push_back(std::make_tuple(sid, access_mode, access));
  return ERROR_SUCCESS;
}

}  // namespace sandbox
