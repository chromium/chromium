// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/restricted_token_utils.h"

#include <aclapi.h>
#include <sddl.h>

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/restricted_token.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/security_level.h"
#include "sandbox/win/src/sid.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

DWORD GetObjectSecurityDescriptor(HANDLE handle,
                                  SECURITY_INFORMATION security_info,
                                  std::vector<char>* security_desc_buffer,
                                  PSECURITY_DESCRIPTOR* security_desc) {
  DWORD last_error = 0;
  DWORD length_needed = 0;

  ::GetKernelObjectSecurity(handle, security_info, nullptr, 0, &length_needed);
  last_error = ::GetLastError();
  if (last_error != ERROR_INSUFFICIENT_BUFFER)
    return last_error;

  security_desc_buffer->resize(length_needed);
  *security_desc =
      reinterpret_cast<PSECURITY_DESCRIPTOR>(security_desc_buffer->data());

  if (!::GetKernelObjectSecurity(handle, security_info, *security_desc,
                                 length_needed, &length_needed)) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

}  // namespace

DWORD CreateRestrictedToken(HANDLE effective_token,
                            TokenLevel security_level,
                            IntegrityLevel integrity_level,
                            TokenType token_type,
                            bool lockdown_default_dacl,
                            base::win::ScopedHandle* token) {
  RestrictedToken restricted_token;
  restricted_token.Init(effective_token);
  if (lockdown_default_dacl)
    restricted_token.SetLockdownDefaultDacl();

  std::vector<std::wstring> privilege_exceptions;
  std::vector<Sid> sid_exceptions;

  bool deny_sids = true;
  bool remove_privileges = true;

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
    case USER_NON_ADMIN: {
      sid_exceptions.push_back(WinBuiltinUsersSid);
      sid_exceptions.push_back(WinWorldSid);
      sid_exceptions.push_back(WinInteractiveSid);
      sid_exceptions.push_back(WinAuthenticatedUserSid);
      privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
      break;
    }
    case USER_RESTRICTED_NON_ADMIN: {
      sid_exceptions.push_back(WinBuiltinUsersSid);
      sid_exceptions.push_back(WinWorldSid);
      sid_exceptions.push_back(WinInteractiveSid);
      sid_exceptions.push_back(WinAuthenticatedUserSid);
      privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
      restricted_token.AddRestrictingSid(WinBuiltinUsersSid);
      restricted_token.AddRestrictingSid(WinWorldSid);
      restricted_token.AddRestrictingSid(WinInteractiveSid);
      restricted_token.AddRestrictingSid(WinAuthenticatedUserSid);
      restricted_token.AddRestrictingSid(WinRestrictedCodeSid);
      restricted_token.AddRestrictingSidCurrentUser();
      restricted_token.AddRestrictingSidLogonSession();
      break;
    }
    case USER_INTERACTIVE: {
      sid_exceptions.push_back(WinBuiltinUsersSid);
      sid_exceptions.push_back(WinWorldSid);
      sid_exceptions.push_back(WinInteractiveSid);
      sid_exceptions.push_back(WinAuthenticatedUserSid);
      privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
      restricted_token.AddRestrictingSid(WinBuiltinUsersSid);
      restricted_token.AddRestrictingSid(WinWorldSid);
      restricted_token.AddRestrictingSid(WinRestrictedCodeSid);
      restricted_token.AddRestrictingSidCurrentUser();
      restricted_token.AddRestrictingSidLogonSession();
      break;
    }
    case USER_LIMITED: {
      sid_exceptions.push_back(WinBuiltinUsersSid);
      sid_exceptions.push_back(WinWorldSid);
      sid_exceptions.push_back(WinInteractiveSid);
      privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
      restricted_token.AddRestrictingSid(WinBuiltinUsersSid);
      restricted_token.AddRestrictingSid(WinWorldSid);
      restricted_token.AddRestrictingSid(WinRestrictedCodeSid);

      // This token has to be able to create objects in BNO.
      // Unfortunately, on Vista+, it needs the current logon sid
      // in the token to achieve this. You should also set the process to be
      // low integrity level so it can't access object created by other
      // processes.
      restricted_token.AddRestrictingSidLogonSession();
      break;
    }
    case USER_RESTRICTED: {
      privilege_exceptions.push_back(SE_CHANGE_NOTIFY_NAME);
      restricted_token.AddUserSidForDenyOnly();
      restricted_token.AddRestrictingSid(WinRestrictedCodeSid);
      break;
    }
    case USER_LOCKDOWN: {
      restricted_token.AddUserSidForDenyOnly();
      restricted_token.AddRestrictingSid(WinNullSid);
      break;
    }
    default: { return ERROR_BAD_ARGUMENTS; }
  }

  DWORD err_code = ERROR_SUCCESS;
  if (deny_sids) {
    err_code = restricted_token.AddAllSidsForDenyOnly(&sid_exceptions);
    if (ERROR_SUCCESS != err_code)
      return err_code;
  }

  if (remove_privileges) {
    err_code = restricted_token.DeleteAllPrivileges(&privilege_exceptions);
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

DWORD SetObjectIntegrityLabel(HANDLE handle,
                              SE_OBJECT_TYPE type,
                              const wchar_t* ace_access,
                              const wchar_t* integrity_level_sid) {
  // Build the SDDL string for the label.
  std::wstring sddl = L"S:(";    // SDDL for a SACL.
  sddl += SDDL_MANDATORY_LABEL;  // Ace Type is "Mandatory Label".
  sddl += L";;";                 // No Ace Flags.
  sddl += ace_access;            // Add the ACE access.
  sddl += L";;;";                // No ObjectType and Inherited Object Type.
  sddl += integrity_level_sid;   // Trustee Sid.
  sddl += L")";

  DWORD error = ERROR_SUCCESS;
  PSECURITY_DESCRIPTOR sec_desc = nullptr;

  PACL sacl = nullptr;
  BOOL sacl_present = false;
  BOOL sacl_defaulted = false;

  if (::ConvertStringSecurityDescriptorToSecurityDescriptorW(
          sddl.c_str(), SDDL_REVISION, &sec_desc, nullptr)) {
    if (::GetSecurityDescriptorSacl(sec_desc, &sacl_present, &sacl,
                                    &sacl_defaulted)) {
      error = ::SetSecurityInfo(handle, type, LABEL_SECURITY_INFORMATION,
                                nullptr, nullptr, nullptr, sacl);
    } else {
      error = ::GetLastError();
    }

    ::LocalFree(sec_desc);
  } else {
    return ::GetLastError();
  }

  return error;
}

const wchar_t* GetIntegrityLevelString(IntegrityLevel integrity_level) {
  switch (integrity_level) {
    case INTEGRITY_LEVEL_SYSTEM:
      return L"S-1-16-16384";
    case INTEGRITY_LEVEL_HIGH:
      return L"S-1-16-12288";
    case INTEGRITY_LEVEL_MEDIUM:
      return L"S-1-16-8192";
    case INTEGRITY_LEVEL_MEDIUM_LOW:
      return L"S-1-16-6144";
    case INTEGRITY_LEVEL_LOW:
      return L"S-1-16-4096";
    case INTEGRITY_LEVEL_BELOW_LOW:
      return L"S-1-16-2048";
    case INTEGRITY_LEVEL_UNTRUSTED:
      return L"S-1-16-0";
    case INTEGRITY_LEVEL_LAST:
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}
DWORD SetTokenIntegrityLevel(HANDLE token, IntegrityLevel integrity_level) {
  const wchar_t* integrity_level_str = GetIntegrityLevelString(integrity_level);
  if (!integrity_level_str) {
    // No mandatory level specified, we don't change it.
    return ERROR_SUCCESS;
  }

  PSID integrity_sid = nullptr;
  if (!::ConvertStringSidToSid(integrity_level_str, &integrity_sid))
    return ::GetLastError();

  TOKEN_MANDATORY_LABEL label = {};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = integrity_sid;

  DWORD size = sizeof(TOKEN_MANDATORY_LABEL) + ::GetLengthSid(integrity_sid);
  bool result = ::SetTokenInformation(token, TokenIntegrityLevel, &label, size);
  auto last_error = ::GetLastError();
  ::LocalFree(integrity_sid);

  return result ? ERROR_SUCCESS : last_error;
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

DWORD HardenTokenIntegrityLevelPolicy(HANDLE token) {
  std::vector<char> security_desc_buffer;
  PSECURITY_DESCRIPTOR security_desc = nullptr;
  DWORD last_error = GetObjectSecurityDescriptor(
      token, LABEL_SECURITY_INFORMATION, &security_desc_buffer, &security_desc);
  if (last_error != ERROR_SUCCESS)
    return last_error;

  PACL sacl = nullptr;
  BOOL sacl_present = false;
  BOOL sacl_defaulted = false;

  if (!::GetSecurityDescriptorSacl(security_desc, &sacl_present, &sacl,
                                   &sacl_defaulted)) {
    return ::GetLastError();
  }

  for (DWORD ace_index = 0; ace_index < sacl->AceCount; ++ace_index) {
    PSYSTEM_MANDATORY_LABEL_ACE ace;

    if (::GetAce(sacl, ace_index, reinterpret_cast<LPVOID*>(&ace)) &&
        ace->Header.AceType == SYSTEM_MANDATORY_LABEL_ACE_TYPE) {
      ace->Mask |= SYSTEM_MANDATORY_LABEL_NO_READ_UP |
                   SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP;
      break;
    }
  }

  if (!::SetKernelObjectSecurity(token, LABEL_SECURITY_INFORMATION,
                                 security_desc))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD HardenProcessIntegrityLevelPolicy() {
  HANDLE token_handle;
  if (!::OpenProcessToken(GetCurrentProcess(), READ_CONTROL | WRITE_OWNER,
                          &token_handle))
    return ::GetLastError();

  base::win::ScopedHandle token(token_handle);

  return HardenTokenIntegrityLevelPolicy(token.Get());
}

DWORD CreateLowBoxToken(HANDLE base_token,
                        TokenType token_type,
                        PSECURITY_CAPABILITIES security_capabilities,
                        PHANDLE saved_handles,
                        DWORD saved_handles_count,
                        base::win::ScopedHandle* token) {
  NtCreateLowBoxToken CreateLowBoxToken = nullptr;
  ResolveNTFunctionPtr("NtCreateLowBoxToken", &CreateLowBoxToken);

  if (base::win::GetVersion() < base::win::Version::WIN8)
    return ERROR_CALL_NOT_IMPLEMENTED;

  if (token_type != PRIMARY && token_type != IMPERSONATION)
    return ERROR_INVALID_PARAMETER;

  if (!token)
    return ERROR_INVALID_PARAMETER;

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
  OBJECT_ATTRIBUTES obj_attr;
  InitializeObjectAttributes(&obj_attr, nullptr, 0, nullptr, nullptr);
  HANDLE token_lowbox = nullptr;

  NTSTATUS status = CreateLowBoxToken(
      &token_lowbox, base_token, TOKEN_ALL_ACCESS, &obj_attr,
      security_capabilities->AppContainerSid,
      security_capabilities->CapabilityCount,
      security_capabilities->Capabilities, saved_handles_count,
      saved_handles_count > 0 ? saved_handles : nullptr);
  if (!NT_SUCCESS(status))
    return GetLastErrorFromNtStatus(status);

  base::win::ScopedHandle token_lowbox_handle(token_lowbox);
  DCHECK(token_lowbox_handle.IsValid());

  // Default from NtCreateLowBoxToken is a Primary token.
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
  std::vector<char> security_desc_buffer;
  PSECURITY_DESCRIPTOR security_desc = nullptr;
  DWORD last_error = GetObjectSecurityDescriptor(
      token_lowbox_handle.Get(), DACL_SECURITY_INFORMATION,
      &security_desc_buffer, &security_desc);
  if (last_error != ERROR_SUCCESS)
    return last_error;

  if (!::SetKernelObjectSecurity(token_for_sd.Get(), DACL_SECURITY_INFORMATION,
                                 security_desc)) {
    return ::GetLastError();
  }

  *token = std::move(token_for_sd);

  return ERROR_SUCCESS;
}

DWORD CreateLowBoxObjectDirectory(PSID lowbox_sid,
                                  bool open_directory,
                                  base::win::ScopedHandle* directory) {
  DWORD session_id = 0;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id))
    return ::GetLastError();

  LPWSTR sid_string = nullptr;
  if (!::ConvertSidToStringSid(lowbox_sid, &sid_string))
    return ::GetLastError();

  std::unique_ptr<wchar_t, LocalFreeDeleter> sid_string_ptr(sid_string);
  std::wstring directory_path = base::StringPrintf(
      L"\\Sessions\\%d\\AppContainerNamedObjects\\%ls", session_id, sid_string);

  NtCreateDirectoryObjectFunction CreateObjectDirectory = nullptr;
  ResolveNTFunctionPtr("NtCreateDirectoryObject", &CreateObjectDirectory);

  OBJECT_ATTRIBUTES obj_attr;
  UNICODE_STRING obj_name;
  DWORD attributes = OBJ_CASE_INSENSITIVE;
  if (open_directory)
    attributes |= OBJ_OPENIF;

  sandbox::InitObjectAttribs(directory_path, attributes, nullptr, &obj_attr,
                             &obj_name, nullptr);

  HANDLE handle = nullptr;
  NTSTATUS status =
      CreateObjectDirectory(&handle, DIRECTORY_ALL_ACCESS, &obj_attr);

  if (!NT_SUCCESS(status))
    return GetLastErrorFromNtStatus(status);
  directory->Set(handle);

  return ERROR_SUCCESS;
}

}  // namespace sandbox
