// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_ACL_H_
#define SANDBOX_WIN_SRC_ACL_H_

#include "base/win/security_descriptor.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/security_level.h"

namespace sandbox {

// Returns the RID associated with a given IntegrityLevel value. This returns
// an empty value if `integrity_level` is set to INTEGRITY_LEVEL_LAST.
std::optional<DWORD> GetIntegrityLevelRid(IntegrityLevel integrity_level);

// Sets the integrity label on a object.
// `handle` should be an open handle with WRITE_OWNER access.
// `object_type` represents the kernel object type of the handle.
// `mandatory_policy` is the mandatory policy to use. This can be zero or more
// of the following bit flags:
// SYSTEM_MANDATORY_LABEL_NO_WRITE_UP   - Block write access.
// SYSTEM_MANDATORY_LABEL_NO_READ_UP    - Block read access.
// SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP - Block execute access.
// `integrity_level` is the level to set.
// If the function succeeds, the return value is ERROR_SUCCESS. If the
// function fails, the return value is the win32 error code corresponding to
// the error.
DWORD SetObjectIntegrityLabel(HANDLE handle,
                              base::win::SecurityObjectType object_type,
                              DWORD mandatory_policy,
                              IntegrityLevel integrity_level);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_ACL_H_
