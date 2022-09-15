// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_RESTRICTED_TOKEN_UTILS_H_
#define SANDBOX_WIN_SRC_RESTRICTED_TOKEN_UTILS_H_

#include "base/win/scoped_handle.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/restricted_token.h"
#include "sandbox/win/src/security_level.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Contains the utility functions to be able to create restricted tokens based
// on a security profiles.

namespace sandbox {

// The type of the token returned by the CreateRestrictedToken and
// CreateLowBoxToken APIs.
enum TokenType { IMPERSONATION = 0, PRIMARY };

// Creates a restricted token from effective token. If it's nullptr then
// effective token of process is used instead. The parameter security_level
// determines how much the token isrestricted. The token_type determines if
// the token will be used as a primarytoken or impersonation token. The
// integrity level of the token is set to |integrity level|.
// |token| is the output value containing the handle of the newly created
// restricted token.
// |lockdown_default_dacl| indicates the token's default DACL should be locked
// down to restrict what other process can open kernel resources created while
// running under the token.
// |unique_restricted_sid| indicates an optional restricted SID to add to the
// token's restricted SID list defined by |security_level|. This allows a
// sandbox process to be grant access to itself and its resources but not
// other sandboxed processes at the same security level.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD CreateRestrictedToken(
    HANDLE effective_token,
    TokenLevel security_level,
    IntegrityLevel integrity_level,
    TokenType token_type,
    bool lockdown_default_dacl,
    const absl::optional<base::win::Sid>& unique_restricted_sid,
    base::win::ScopedHandle* token);

// Sets the integrity level on a token. If the integrity level that you specify
// is greater than the current integrity level, the function will fail.
// |token| must be a token handle with TOKEN_ADJUST_DEFAULTS access.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD SetTokenIntegrityLevel(HANDLE token, IntegrityLevel integrity_level);

// Sets the integrity level on the current process token. If the integrity level
// that you specify is greater than the current integrity level, the function
// will fail.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD SetProcessIntegrityLevel(IntegrityLevel integrity_level);

// Hardens the integrity level policy on a token. Specifically it sets the
// policy to block read and execute so that a lower privileged process cannot
// open the token for impersonate or duplicate permissions. This should limit
// potential security holes.
// |token| must be a token handle with READ_CONTROL and WRITE_OWNER access.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD HardenTokenIntegrityLevelPolicy(HANDLE token);

// Hardens the integrity level policy on the current process. Specifically it
// sets the policy to block read and execute so that a lower privileged process
// cannot open the token for impersonate or duplicate permissions. This should
// limit potential security holes.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD HardenProcessIntegrityLevelPolicy();

// Create a lowbox token. This is not valid prior to Windows 8.
// |base_token| a base token to derive the lowbox token from. Can be nullptr.
// |security_capabilities| list of LowBox capabilities to use when creating the
// token.
// |token| is the output value containing the handle of the newly created
// restricted token.
// |lockdown_default_dacl| indicates the token's default DACL should be locked
// down to restrict what other process can open kernel resources created while
// running under the token.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD CreateLowBoxToken(HANDLE base_token,
                        TokenType token_type,
                        SECURITY_CAPABILITIES* security_capabilities,
                        base::win::ScopedHandle* token);

// Returns true if a low IL token can access the current desktop, false
// otherwise.
bool CanLowIntegrityAccessDesktop();

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_RESTRICTED_TOKEN_UTILS_H_
