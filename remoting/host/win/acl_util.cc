// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/acl_util.h"

#include <windows.h>

#include "base/logging.h"
#include "base/win/scoped_handle.h"
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

bool AddTokenAccessRightForWellKnownSid(base::win::WellKnownSid well_known_sid,
                                        DWORD new_right) {
  HANDLE token_handle = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_QUERY | WRITE_DAC | READ_CONTROL,
                          &token_handle)) {
    PLOG(ERROR) << "Failed to open current process token";
    return false;
  }
  base::win::ScopedHandle token(token_handle);

  auto sd = base::win::SecurityDescriptor::FromHandle(
      token.get(), base::win::SecurityObjectType::kKernel,
      DACL_SECURITY_INFORMATION);
  if (!sd) {
    PLOG(ERROR)
        << "Failed to read security descriptor of current process token";
    return false;
  }
  if (!sd->SetDaclEntry(well_known_sid, base::win::SecurityAccessMode::kGrant,
                        new_right,
                        /* inheritance= */ 0)) {
    PLOG(ERROR) << "Failed to set DACL entry on token security descriptor";
    return false;
  }
  if (!sd->WriteToHandle(token.get(), base::win::SecurityObjectType::kKernel,
                         DACL_SECURITY_INFORMATION)) {
    PLOG(ERROR)
        << "Failed to write security descriptor to current process token";
    return false;
  }
  return true;
}

}  // namespace remoting
