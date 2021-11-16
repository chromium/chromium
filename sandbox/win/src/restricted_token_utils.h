// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_RESTRICTED_TOKEN_UTILS_H_
#define SANDBOX_WIN_SRC_RESTRICTED_TOKEN_UTILS_H_

#include <windows.h>

#include "base/win/scoped_handle.h"
#include "base/win/sid.h"
#include "sandbox/win/src/restricted_token.h"
#include "sandbox/win/src/security_level.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Contains the utility functions to be able to create restricted tokens based
// on a security profiles.

namespace sandbox {

// The type of the token returned by the CreateNakedToken.
enum TokenType { IMPERSONATION = 0, PRIMARY };

// Creates a restricted token from effective token. If it's nullptr then
// effective token of process is used instead. The parameter security_level
// determines how much the token isrestricted. The token_type determines if
// the token will be used as a primarytoken or impersonation token. The
// integrity level of the token is set to |integrity level| on Vista only.
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

// Sets the integrity level on a token. This is only valid on Vista. It returns
// without failing on XP. If the integrity level that you specify is greater
// than the current integrity level, the function will fail.
DWORD SetTokenIntegrityLevel(HANDLE token, IntegrityLevel integrity_level);

// Returns the integrity level SDDL string associated with a given
// IntegrityLevel value.
const wchar_t* GetIntegrityLevelString(IntegrityLevel integrity_level);

// Sets the integrity level on the current process on Vista. It returns without
// failing on XP. If the integrity level that you specify is greater than the
// current integrity level, the function will fail.
DWORD SetProcessIntegrityLevel(IntegrityLevel integrity_level);

// Hardens the integrity level policy on a token. This is only valid on Win 7
// and above. Specifically it sets the policy to block read and execute so
// that a lower privileged process cannot open the token for impersonate or
// duplicate permissions. This should limit potential security holes.
DWORD HardenTokenIntegrityLevelPolicy(HANDLE token);

// Hardens the integrity level policy on the current process. This is only
// valid on Win 7 and above. Specifically it sets the policy to block read
// and execute so that a lower privileged process cannot open the token for
// impersonate or duplicate permissions. This should limit potential security
// holes.
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
DWORD CreateLowBoxToken(HANDLE base_token,
                        TokenType token_type,
                        PSECURITY_CAPABILITIES security_capabilities,
                        PHANDLE saved_handles,
                        DWORD saved_handles_count,
                        base::win::ScopedHandle* token);

// Create a lowbox object directory token. This is not valid prior to Windows 8.
// This returns the Win32 error code from the operation.
// |lowbox_sid| the SID for the LowBox.
// |open_directory| open the directory if it already exists.
// |directory| is the output value for the directory object.
DWORD CreateLowBoxObjectDirectory(PSID lowbox_sid,
                                  bool open_directory,
                                  base::win::ScopedHandle* directory);

// Returns true if a low IL token can access the current desktop, false
// otherwise.
bool CanLowIntegrityAccessDesktop();

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_RESTRICTED_TOKEN_UTILS_H_
