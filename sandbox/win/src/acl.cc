// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/acl.h"

#include <windows.h>

#include "base/notreached.h"

namespace sandbox {

bool AddKnownSidToObject(HANDLE object,
                         base::win::SecurityObjectType object_type,
                         const base::win::Sid& sid,
                         base::win::SecurityAccessMode access_mode,
                         ACCESS_MASK access) {
  absl::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(object, object_type,
                                                DACL_SECURITY_INFORMATION);
  if (!sd) {
    return false;
  }

  if (!sd->SetDaclEntry(sid, access_mode, access, 0)) {
    return false;
  }

  return sd->WriteToHandle(object, object_type, DACL_SECURITY_INFORMATION);
}

bool AddKnownSidToObject(HANDLE object,
                         base::win::SecurityObjectType object_type,
                         base::win::WellKnownSid known_sid,
                         base::win::SecurityAccessMode access_mode,
                         ACCESS_MASK access) {
  return AddKnownSidToObject(object, object_type, base::win::Sid(known_sid),
                             access_mode, access);
}

bool ReplacePackageSidInDacl(HANDLE object,
                             base::win::SecurityObjectType object_type,
                             const base::win::Sid& package_sid,
                             ACCESS_MASK access) {
  if (!AddKnownSidToObject(object, object_type, package_sid,
                           base::win::SecurityAccessMode::kRevoke, 0)) {
    return false;
  }

  return AddKnownSidToObject(object, object_type,
                             base::win::WellKnownSid::kAllApplicationPackages,
                             base::win::SecurityAccessMode::kGrant, access);
}

absl::optional<DWORD> GetIntegrityLevelRid(IntegrityLevel integrity_level) {
  switch (integrity_level) {
    case INTEGRITY_LEVEL_SYSTEM:
      return DWORD{SECURITY_MANDATORY_SYSTEM_RID};
    case INTEGRITY_LEVEL_HIGH:
      return DWORD{SECURITY_MANDATORY_HIGH_RID};
    case INTEGRITY_LEVEL_MEDIUM:
      return DWORD{SECURITY_MANDATORY_MEDIUM_RID};
    case INTEGRITY_LEVEL_MEDIUM_LOW:
      return DWORD{SECURITY_MANDATORY_MEDIUM_RID - 2048};
    case INTEGRITY_LEVEL_LOW:
      return DWORD{SECURITY_MANDATORY_LOW_RID};
    case INTEGRITY_LEVEL_BELOW_LOW:
      return DWORD{SECURITY_MANDATORY_LOW_RID - 2048};
    case INTEGRITY_LEVEL_UNTRUSTED:
      return DWORD{SECURITY_MANDATORY_UNTRUSTED_RID};
    case INTEGRITY_LEVEL_LAST:
      return absl::nullopt;
  }

  NOTREACHED();
  return absl::nullopt;
}

DWORD SetObjectIntegrityLabel(HANDLE handle,
                              base::win::SecurityObjectType object_type,
                              DWORD mandatory_policy,
                              IntegrityLevel integrity_level) {
  absl::optional<DWORD> value = GetIntegrityLevelRid(integrity_level);
  if (!value) {
    return ERROR_INVALID_SID;
  }

  base::win::SecurityDescriptor sd;
  if (!sd.SetMandatoryLabel(*value, 0, mandatory_policy)) {
    return ::GetLastError();
  }

  if (!sd.WriteToHandle(handle, object_type, LABEL_SECURITY_INFORMATION)) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

}  // namespace sandbox
