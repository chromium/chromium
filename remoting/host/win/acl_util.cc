// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/acl_util.h"

#include <windows.h>

#include "base/logging.h"
#include "base/win/security_descriptor.h"

namespace remoting {

bool AddProcessAccessRightForWellKnownSid(
    base::win::WellKnownSid well_known_sid,
    DWORD new_right) {
  auto sd = base::win::SecurityDescriptor::FromHandle(
      GetCurrentProcess(), base::win::SecurityObjectType::kKernel,
      DACL_SECURITY_INFORMATION);
  if (!sd) {
    PLOG(ERROR) << "Failed to read security descriptor of current process";
    return false;
  }
  if (!sd->SetDaclEntry(well_known_sid, base::win::SecurityAccessMode::kGrant,
                        new_right,
                        /* inheritance= */ 0)) {
    PLOG(ERROR) << "Failed to set DACL entry on security descriptor";
    return false;
  }
  if (!sd->WriteToHandle(GetCurrentProcess(),
                         base::win::SecurityObjectType::kKernel,
                         DACL_SECURITY_INFORMATION)) {
    PLOG(ERROR) << "Failed to write security descriptor to current process";
    return false;
  }
  return true;
}

}  // namespace remoting
