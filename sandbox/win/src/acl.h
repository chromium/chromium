// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_ACL_H_
#define SANDBOX_WIN_SRC_ACL_H_

#include "base/memory/free_deleter.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"

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

// Sets the integrity label on a object handle.
DWORD SetObjectIntegrityLabel(HANDLE handle,
                              SecurityObjectType type,
                              const wchar_t* ace_access,
                              const wchar_t* integrity_level_sid);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_ACL_H_
