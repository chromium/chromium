// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_RESTRICTED_TOKEN_UTILS_H_
#define SANDBOX_WIN_SRC_RESTRICTED_TOKEN_UTILS_H_

#include <vector>

#include "base/win/access_token.h"
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
enum class TokenType { kImpersonation, kPrimary };

// Creates a restricted token from the current process' token. The parameter
// `security_level` determines how much the token is restricted.
// `token_type` determines if the token will be used as a primary or
// impersonation token.
//  `integrity level` set the token's integrity level.
// `lockdown_default_dacl` indicates the token's default DACL should be locked
// down to restrict what other process can open kernel resources created while
// running under the token.
// `unique_restricted_sid` indicates an optional restricted SID to add to the
// token's restricted SID list defined by `security_level`. This allows a
// sandbox process to be grant access to itself and its resources but not
// other sandboxed processes at the same security level.
// If the function succeeds, the return value is the restricted token. If it
// fails then the return value is empty.
absl::optional<base::win::AccessToken> CreateRestrictedToken(
    TokenLevel security_level,
    IntegrityLevel integrity_level,
    TokenType token_type,
    bool lockdown_default_dacl,
    const absl::optional<base::win::Sid>& unique_restricted_sid);

// Hardens the integrity level policy on a token. Specifically it sets the
// policy to block read and execute so that a lower privileged process cannot
// open the token for impersonate or duplicate permissions. This should limit
// potential security holes.
// `token` must be a token with READ_CONTROL and WRITE_OWNER access.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD HardenTokenIntegrityLevelPolicy(const base::win::AccessToken& token);

// Create a lowbox token.
// `base_token` a base token to derive the lowbox token from. Can be nullptr.
// `token_type` specify to create either a primary or impersonation token.
// `package_sid` is the AppContainer package SID.
// `capabilities` is the list of AppContainer capabilities.
// `token` is the output value containing the handle of the newly created
// If the function succeeds, the return value is true.
bool CreateLowBoxToken(HANDLE base_token,
                       TokenType token_type,
                       const base::win::Sid& package_sid,
                       const std::vector<base::win::Sid>& capabilities,
                       base::win::ScopedHandle* token);

// Returns true if a low IL token can access the current desktop, false
// otherwise.
bool CanLowIntegrityAccessDesktop();

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_RESTRICTED_TOKEN_UTILS_H_
