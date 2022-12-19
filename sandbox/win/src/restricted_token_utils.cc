// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/restricted_token_utils.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "sandbox/win/src/restricted_token.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/security_level.h"
#include "sandbox/win/src/win_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sandbox {

namespace {

void AddSidException(std::vector<base::win::Sid>& sids,
                     base::win::WellKnownSid known_sid) {
  sids.push_back(base::win::Sid::FromKnownSid(known_sid));
}

typedef BOOL(WINAPI* CreateAppContainerTokenFunction)(
    HANDLE TokenHandle,
    PSECURITY_CAPABILITIES SecurityCapabilities,
    PHANDLE OutToken);

}  // namespace

DWORD CreateRestrictedToken(
    HANDLE effective_token,
    TokenLevel security_level,
    IntegrityLevel integrity_level,
    TokenType token_type,
    bool lockdown_default_dacl,
    const absl::optional<base::win::Sid>& unique_restricted_sid,
    base::win::ScopedHandle* token) {
  RestrictedToken restricted_token;
  restricted_token.Init(effective_token);
  if (lockdown_default_dacl)
    restricted_token.SetLockdownDefaultDacl();
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
    case USER_UNPROTECTED: {
      deny_sids = false;
      remove_privileges = false;
      break;
    }
    case USER_RESTRICTED_SAME_ACCESS: {
      deny_sids = false;
      remove_privileges = false;

      unsigned err_code = restricted_token.AddRestrictingSidAllSids();
      if (ERROR_SUCCESS != err_code)
        return err_code;

      break;
    }
    case USER_RESTRICTED_NON_ADMIN: {
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
      if (unique_restricted_sid)
        restricted_token.AddRestrictingSid(*unique_restricted_sid);
      break;
    }
    case USER_INTERACTIVE: {
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
      if (unique_restricted_sid)
        restricted_token.AddRestrictingSid(*unique_restricted_sid);
      break;
    }
    case USER_LIMITED: {
      AddSidException(sid_exceptions, base::win::WellKnownSid::kBuiltinUsers);
      AddSidException(sid_exceptions, base::win::WellKnownSid::kWorld);
      AddSidException(sid_exceptions, base::win::WellKnownSid::kInteractive);
      restricted_token.AddRestrictingSid(
          base::win::WellKnownSid::kBuiltinUsers);
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kWorld);
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kRestricted);
      if (unique_restricted_sid)
        restricted_token.AddRestrictingSid(*unique_restricted_sid);

      // This token has to be able to create objects in BNO, it needs the
      // current logon sid in the token to achieve this. You should also set the
      // process to be low integrity level so it can't access object created by
      // other processes.
      restricted_token.AddRestrictingSidLogonSession();
      break;
    }
    case USER_RESTRICTED: {
      restricted_token.AddUserSidForDenyOnly();
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kRestricted);
      if (unique_restricted_sid)
        restricted_token.AddRestrictingSid(*unique_restricted_sid);
      break;
    }
    case USER_LOCKDOWN: {
      remove_traverse_privilege = true;
      restricted_token.AddUserSidForDenyOnly();
      restricted_token.AddRestrictingSid(base::win::WellKnownSid::kNull);
      if (unique_restricted_sid)
        restricted_token.AddRestrictingSid(*unique_restricted_sid);
      break;
    }
    case USER_LAST:
      return ERROR_BAD_ARGUMENTS;
  }

  DWORD err_code = ERROR_SUCCESS;
  if (deny_sids) {
    err_code = restricted_token.AddAllSidsForDenyOnly(sid_exceptions);
    if (ERROR_SUCCESS != err_code)
      return err_code;
  }

  if (remove_privileges) {
    err_code = restricted_token.DeleteAllPrivileges(remove_traverse_privilege);
    if (ERROR_SUCCESS != err_code)
      return err_code;
  }

  restricted_token.SetIntegrityLevel(integrity_level);

  switch (token_type) {
    case PRIMARY: {
      err_code = restricted_token.GetRestrictedToken(token);
      break;
    }
    case IMPERSONATION: {
      err_code = restricted_token.GetRestrictedTokenForImpersonation(token);
      break;
    }
    default: {
      err_code = ERROR_BAD_ARGUMENTS;
      break;
    }
  }

  return err_code;
}

DWORD SetTokenIntegrityLevel(HANDLE token, IntegrityLevel integrity_level) {
  absl::optional<base::win::Sid> sid = GetIntegrityLevelSid(integrity_level);
  if (!sid) {
    // No mandatory level specified, we don't change it.
    return ERROR_SUCCESS;
  }

  TOKEN_MANDATORY_LABEL label = {};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = sid->GetPSID();

  if (!::SetTokenInformation(token, TokenIntegrityLevel, &label, sizeof(label)))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD SetProcessIntegrityLevel(IntegrityLevel integrity_level) {
  // We don't check for an invalid level here because we'll just let it
  // fail on the SetTokenIntegrityLevel call later on.
  if (integrity_level == INTEGRITY_LEVEL_LAST) {
    // No mandatory level specified, we don't change it.
    return ERROR_SUCCESS;
  }

  HANDLE token_handle;
  if (!::OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_DEFAULT,
                          &token_handle))
    return ::GetLastError();

  base::win::ScopedHandle token(token_handle);

  return SetTokenIntegrityLevel(token.Get(), integrity_level);
}

DWORD HardenTokenIntegrityLevelPolicy(const base::win::AccessToken& token) {
  absl::optional<base::win::SecurityDescriptor> sd =
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

DWORD CreateLowBoxToken(HANDLE base_token,
                        TokenType token_type,
                        SECURITY_CAPABILITIES* security_capabilities,
                        base::win::ScopedHandle* token) {
  if (token_type != PRIMARY && token_type != IMPERSONATION)
    return ERROR_INVALID_PARAMETER;

  if (!token)
    return ERROR_INVALID_PARAMETER;

  CreateAppContainerTokenFunction CreateAppContainerToken =
      reinterpret_cast<CreateAppContainerTokenFunction>(::GetProcAddress(
          ::GetModuleHandle(L"kernelbase.dll"), "CreateAppContainerToken"));
  if (!CreateAppContainerToken)
    return ::GetLastError();

  base::win::ScopedHandle base_token_handle;
  if (!base_token) {
    HANDLE process_token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                            &process_token)) {
      return ::GetLastError();
    }
    base_token_handle.Set(process_token);
    base_token = process_token;
  }
  HANDLE token_lowbox = nullptr;
  if (!CreateAppContainerToken(base_token, security_capabilities,
                               &token_lowbox)) {
    return ::GetLastError();
  }

  base::win::ScopedHandle token_lowbox_handle(token_lowbox);
  DCHECK(token_lowbox_handle.IsValid());

  // Default from CreateAppContainerToken is a Primary token.
  if (token_type == PRIMARY) {
    *token = std::move(token_lowbox_handle);
    return ERROR_SUCCESS;
  }

  HANDLE dup_handle = nullptr;
  if (!::DuplicateTokenEx(token_lowbox_handle.Get(), TOKEN_ALL_ACCESS, nullptr,
                          ::SecurityImpersonation, ::TokenImpersonation,
                          &dup_handle)) {
    return ::GetLastError();
  }

  // Copy security descriptor from primary token as the new object will have
  // the DACL from the current token's default DACL.
  base::win::ScopedHandle token_for_sd(dup_handle);

  absl::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          token_lowbox_handle.get(), base::win::SecurityObjectType::kKernel,
          DACL_SECURITY_INFORMATION);
  if (!sd) {
    return ::GetLastError();
  }

  if (!sd->WriteToHandle(token_for_sd.get(),
                         base::win::SecurityObjectType::kKernel,
                         DACL_SECURITY_INFORMATION)) {
    return ::GetLastError();
  }

  *token = std::move(token_for_sd);

  return ERROR_SUCCESS;
}

bool CanLowIntegrityAccessDesktop() {
  // Access required for UI thread to initialize (when user32.dll loads without
  // win32k lockdown).
  DWORD desired_access = DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS;

  // Desktop is inherited by child process unless overridden, e.g. by sandbox.
  HDESK hdesk = ::GetThreadDesktop(GetCurrentThreadId());
  absl::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          hdesk, base::win::SecurityObjectType::kDesktop,
          OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
              DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION);
  if (!sd) {
    return false;
  }

  absl::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess(/*impersonation=*/true,
                                                 TOKEN_ADJUST_DEFAULT);
  if (!token) {
    return false;
  }

  absl::optional<base::win::AccessCheckResult> result;
  // The token should still succeed before lowered, even if the lowered token
  // fails.
  DCHECK((result = sd->AccessCheck(*token, desired_access,
                                   base::win::SecurityObjectType::kDesktop)) &&
         result->access_status);
  if (!token->SetIntegrityLevel(SECURITY_MANDATORY_LOW_RID)) {
    return false;
  }

  result = sd->AccessCheck(*token, desired_access,
                           base::win::SecurityObjectType::kDesktop);
  return result && result->access_status;
}

}  // namespace sandbox
