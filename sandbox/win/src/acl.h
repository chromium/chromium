// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_ACL_H_
#define SANDBOX_WIN_SRC_ACL_H_

#include "base/memory/free_deleter.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/security_level.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sandbox {

// Represents the type of Windows kernel object to apply the operation to.
enum class SecurityObjectType { kFile, kRegistry, kWindow, kKernel };

// Represents the type of access operation to perform on an ACL.
enum class SecurityAccessMode { kGrant, kSet, kDeny, kRevoke };

// Adds an ACE represented by |sid| and |access| with |access_mode| to the
// default dacl present in the token.
bool AddSidToDefaultDacl(HANDLE token,
                         const base::win::Sid& sid,
                         SecurityAccessMode access_mode,
                         ACCESS_MASK access);

// Adds an ACE represented by |known_sid| and |access| with |access_mode| to the
// default dacl present in the token.
bool AddSidToDefaultDacl(HANDLE token,
                         base::win::WellKnownSid known_sid,
                         SecurityAccessMode access_mode,
                         ACCESS_MASK access);

// Revokes access to the logon SID for the default dacl present in the token.
bool RevokeLogonSidFromDefaultDacl(HANDLE token);

// Adds an ACE represented by the user sid and |access| to the default dacl
// present in the token.
bool AddUserSidToDefaultDacl(HANDLE token, ACCESS_MASK access);

// Adds an ACE represented by |known_sid|, |access_mode|, and |access| to
// the dacl of the kernel object referenced by |object| and of |object_type|.
bool AddKnownSidToObject(HANDLE object,
                         SecurityObjectType object_type,
                         base::win::WellKnownSid known_sid,
                         SecurityAccessMode access_mode,
                         ACCESS_MASK access);

// Adds an ACE represented by |sid|, |access_mode|, and |access| to
// the dacl of the kernel object referenced by |object| and of |object_type|.
bool AddKnownSidToObject(HANDLE object,
                         SecurityObjectType object_type,
                         const base::win::Sid& sid,
                         SecurityAccessMode access_mode,
                         ACCESS_MASK access);

// Replace package SID in DACL to the "any package" SID. It allows Low-IL
// tokens to open the object which is important for warm up when using renderer
// AppContainer.
bool ReplacePackageSidInDacl(HANDLE object,
                             SecurityObjectType object_type,
                             const base::win::Sid& package_sid,
                             ACCESS_MASK access);

// Returns the Sid associated with a given IntegrityLevel value. This returns
// an empty value if |integrity_level| is set to INTEGRITY_LEVEL_LAST.
absl::optional<base::win::Sid> GetIntegrityLevelSid(
    IntegrityLevel integrity_level);

// Sets the integrity label on a object.
// |handle| should be an open handle with WRITE_OWNER access.
// |object_type| represents the kernel object type of the handle.
// |mandatory_policy| is the mandatory policy to use. This can be zero or more
// of the following bit flags:
// SYSTEM_MANDATORY_LABEL_NO_WRITE_UP   - Block write access.
// SYSTEM_MANDATORY_LABEL_NO_READ_UP    - Block read access.
// SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP - Block execute access.
// |integrity_level| is the level to set.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD SetObjectIntegrityLabel(HANDLE handle,
                              SecurityObjectType object_type,
                              DWORD mandatory_policy,
                              IntegrityLevel integrity_level);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_ACL_H_
