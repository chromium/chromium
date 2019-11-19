// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_ACL_H_
#define SANDBOX_SRC_ACL_H_

#include <accctrl.h>
#include <windows.h>

#include <memory>

#include "base/memory/free_deleter.h"
#include "sandbox/win/src/sid.h"

namespace sandbox {

// Returns the default dacl from the token passed in.
bool GetDefaultDacl(
    HANDLE token,
    std::unique_ptr<TOKEN_DEFAULT_DACL, base::FreeDeleter>* default_dacl);

// Appends an ACE represented by |sid|, |access_mode|, and |access| to
// |old_dacl|. If the function succeeds, new_dacl contains the new dacl and
// must be freed using LocalFree.
bool AddSidToDacl(const Sid& sid,
                  ACL* old_dacl,
                  ACCESS_MODE access_mode,
                  ACCESS_MASK access,
                  ACL** new_dacl);

// Adds an ACE represented by |sid| and |access| with |access_mode| to the
// default dacl present in the token.
bool AddSidToDefaultDacl(HANDLE token,
                         const Sid& sid,
                         ACCESS_MODE access_mode,
                         ACCESS_MASK access);

// Revokes access to the logon SID for the default dacl present in the token.
bool RevokeLogonSidFromDefaultDacl(HANDLE token);

// Adds an ACE represented by the user sid and |access| to the default dacl
// present in the token.
bool AddUserSidToDefaultDacl(HANDLE token, ACCESS_MASK access);

// Adds an ACE represented by |known_sid|, |access_mode|, and |access| to
// the dacl of the kernel object referenced by |object| and of |object_type|.
bool AddKnownSidToObject(HANDLE object,
                         SE_OBJECT_TYPE object_type,
                         const Sid& sid,
                         ACCESS_MODE access_mode,
                         ACCESS_MASK access);

// Replace package SID in DACL to the "any package" SID. It allows Low-IL
// tokens to open the object which is important for warm up when using renderer
// AppContainer.
bool ReplacePackageSidInDacl(HANDLE object,
                             SE_OBJECT_TYPE object_type,
                             const Sid& package_sid,
                             ACCESS_MASK access);

}  // namespace sandbox

#endif  // SANDBOX_SRC_ACL_H_
