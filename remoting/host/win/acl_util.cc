// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/acl_util.h"

#include <aclapi.h>
#include <windows.h>

#include "base/logging.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"

namespace remoting {

bool AddProcessAccessRightForWellKnownSid(WELL_KNOWN_SID_TYPE type,
                                          DWORD new_right) {
  // Open a handle for the current process, read the current DACL, update it,
  // and write it back.  This will add |new_right| to the current process.
  base::win::ScopedHandle process_handle(OpenProcess(READ_CONTROL | WRITE_DAC,
                                                     /*bInheritHandle=*/FALSE,
                                                     GetCurrentProcessId()));
  if (!process_handle.IsValid()) {
    PLOG(ERROR) << "OpenProcess() failed!";
    return false;
  }

  PSECURITY_DESCRIPTOR descriptor_ptr = nullptr;
  // |old_dacl_ptr| is a pointer into the opaque |descriptor_ptr| struct, don't
  // free it.
  PACL old_dacl_ptr = nullptr;
  PACL new_dacl_ptr = nullptr;

  if (GetSecurityInfo(process_handle.Get(), SE_KERNEL_OBJECT,
                      DACL_SECURITY_INFORMATION,
                      /*ppsidOwner=*/nullptr,
                      /*ppsidGroup=*/nullptr, &old_dacl_ptr,
                      /*ppSacl=*/nullptr, &descriptor_ptr) != ERROR_SUCCESS) {
    PLOG(ERROR) << "GetSecurityInfo() failed!";
    return false;
  }

  base::win::ScopedLocalAlloc descriptor =
      base::win::TakeLocalAlloc(descriptor_ptr);
  BYTE buffer[SECURITY_MAX_SID_SIZE] = {0};
  DWORD buffer_size = SECURITY_MAX_SID_SIZE;
  if (!CreateWellKnownSid(type, /*DomainSid=*/nullptr, buffer, &buffer_size)) {
    PLOG(ERROR) << "CreateWellKnownSid() failed!";
    return false;
  }

  SID* sid = reinterpret_cast<SID*>(buffer);
  EXPLICIT_ACCESS new_access = {0};
  new_access.grfAccessMode = GRANT_ACCESS;
  new_access.grfAccessPermissions = new_right;
  new_access.grfInheritance = NO_INHERITANCE;

  new_access.Trustee.pMultipleTrustee = nullptr;
  new_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
  new_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  new_access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(sid);

  if (SetEntriesInAcl(1, &new_access, old_dacl_ptr, &new_dacl_ptr) !=
      ERROR_SUCCESS) {
    PLOG(ERROR) << "SetEntriesInAcl() failed!";
    return false;
  }
  base::win::ScopedLocalAllocTyped<ACL> new_dacl =
      base::win::TakeLocalAlloc(new_dacl_ptr);

  bool right_added = true;
  if (SetSecurityInfo(process_handle.Get(), SE_KERNEL_OBJECT,
                      DACL_SECURITY_INFORMATION,
                      /*psidOwner=*/nullptr,
                      /*psidGroup=*/nullptr, new_dacl.get(),
                      /*ppSacl=*/nullptr) != ERROR_SUCCESS) {
    PLOG(ERROR) << "SetSecurityInfo() failed!";
    right_added = false;
  }

  return right_added;
}

}  // namespace remoting
