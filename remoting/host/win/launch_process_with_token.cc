// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/launch_process_with_token.h"

#include <windows.h>

#include <utility>

#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/startup_information.h"

using base::win::ScopedHandle;

namespace {

// Copies the process token making it a primary impersonation token.
// The returned handle will have |desired_access| rights.
bool CopyProcessToken(DWORD desired_access, ScopedHandle* token_out) {
  HANDLE temp_handle;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | desired_access,
                        &temp_handle)) {
    PLOG(ERROR) << "Failed to open process token";
    return false;
  }
  ScopedHandle process_token(temp_handle);

  if (!DuplicateTokenEx(process_token.Get(), desired_access, nullptr,
                        SecurityImpersonation, TokenPrimary, &temp_handle)) {
    PLOG(ERROR) << "Failed to duplicate the process token";
    return false;
  }

  token_out->Set(temp_handle);
  return true;
}

// Creates a copy of the current process with SE_TCB_NAME privilege enabled.
bool CreatePrivilegedToken(ScopedHandle* token_out) {
  ScopedHandle privileged_token;
  DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
                         TOKEN_DUPLICATE | TOKEN_QUERY;
  if (!CopyProcessToken(desired_access, &privileged_token)) {
    return false;
  }

  // Get the LUID for the SE_TCB_NAME privilege.
  TOKEN_PRIVILEGES state;
  state.PrivilegeCount = 1;
  state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  if (!LookupPrivilegeValue(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid)) {
    PLOG(ERROR) << "Failed to lookup the LUID for the SE_TCB_NAME privilege";
    return false;
  }

  // Enable the SE_TCB_NAME privilege.
  if (!AdjustTokenPrivileges(privileged_token.Get(), FALSE, &state, 0, nullptr,
                             0)) {
    PLOG(ERROR) << "Failed to enable SE_TCB_NAME privilege in a token";
    return false;
  }

  *token_out = std::move(privileged_token);
  return true;
}

}  // namespace

namespace remoting {

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool CreateSessionToken(uint32_t session_id, ScopedHandle* token_out) {
  ScopedHandle session_token;
  DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
                         TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;
  if (!CopyProcessToken(desired_access, &session_token)) {
    return false;
  }

  // Temporarily enable the SE_TCB_NAME privilege as it is required by
  // SetTokenInformation(TokenSessionId).
  ScopedHandle privileged_token;
  if (!CreatePrivilegedToken(&privileged_token)) {
    return false;
  }
  if (!ImpersonateLoggedOnUser(privileged_token.Get())) {
    PLOG(ERROR) << "Failed to impersonate the privileged token";
    return false;
  }

  // Change the session ID of the token.
  DWORD new_session_id = session_id;
  if (!SetTokenInformation(session_token.Get(), TokenSessionId, &new_session_id,
                           sizeof(new_session_id))) {
    PLOG(ERROR) << "Failed to change session ID of a token";

    // Revert to the default token.
    CHECK(RevertToSelf());
    return false;
  }

  // Revert to the default token.
  CHECK(RevertToSelf());

  *token_out = std::move(session_token);
  return true;
}

bool LaunchProcessWithToken(
    const base::FilePath& binary,
    const base::CommandLine::StringType& command_line,
    HANDLE user_token,
    SECURITY_ATTRIBUTES* process_attributes,
    SECURITY_ATTRIBUTES* thread_attributes,
    const base::HandlesToInheritVector& handles_to_inherit,
    DWORD creation_flags,
    const wchar_t* desktop_name,
    ScopedHandle* process_out,
    ScopedHandle* thread_out) {
  base::FilePath::StringType application_name = binary.value();

  base::win::StartupInformation startup_info_wrapper;
  STARTUPINFO* startup_info = startup_info_wrapper.startup_info();
  if (desktop_name) {
    startup_info->lpDesktop = const_cast<wchar_t*>(desktop_name);
  }

  bool inherit_handles = false;
  if (!handles_to_inherit.empty()) {
    if (handles_to_inherit.size() >
        std::numeric_limits<DWORD>::max() / sizeof(HANDLE)) {
      DLOG(ERROR) << "Too many handles to inherit.";
      return false;
    }

    // Ensure the handles can be inherited.
    for (HANDLE handle : handles_to_inherit) {
      BOOL result = SetHandleInformation(handle, HANDLE_FLAG_INHERIT,
                                         HANDLE_FLAG_INHERIT);
      PCHECK(result);
    }

    if (!startup_info_wrapper.InitializeProcThreadAttributeList(
            /* attribute_count= */ 1)) {
      PLOG(ERROR) << "InitializeProcThreadAttributeList()";
      return false;
    }

    if (!startup_info_wrapper.UpdateProcThreadAttribute(
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            const_cast<HANDLE*>(&handles_to_inherit.at(0)),
            static_cast<DWORD>(handles_to_inherit.size() * sizeof(HANDLE)))) {
      PLOG(ERROR) << "UpdateProcThreadAttribute()";
      return false;
    }

    inherit_handles = true;
    creation_flags |= EXTENDED_STARTUPINFO_PRESENT;
  }
  PROCESS_INFORMATION temp_process_info = {};
  BOOL result = CreateProcessAsUser(user_token, application_name.c_str(),
                                    const_cast<LPWSTR>(command_line.c_str()),
                                    process_attributes, thread_attributes,
                                    inherit_handles, creation_flags, nullptr,
                                    nullptr, startup_info, &temp_process_info);

  if (!result) {
    PLOG(ERROR) << "Failed to launch a process with a user token";
    return false;
  }

  base::win::ScopedProcessInformation process_info(temp_process_info);

  CHECK(process_info.IsValid());
  process_out->Set(process_info.TakeProcessHandle());
  thread_out->Set(process_info.TakeThreadHandle());
  return true;
}

}  // namespace remoting
