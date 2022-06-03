// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_permissions.h"

#include "base/check.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "ppapi/shared_impl/ppapi_switches.h"

namespace ppapi {

PpapiPermissions::PpapiPermissions() : permissions_(0) {}

PpapiPermissions::PpapiPermissions(uint32_t perms) : permissions_(perms) {}

PpapiPermissions::~PpapiPermissions() {}

// static
PpapiPermissions PpapiPermissions::AllPermissions() {
  return PpapiPermissions(PERMISSION_ALL_BITS);
}

// static
PpapiPermissions PpapiPermissions::GetForCommandLine(uint32_t base_perms) {
  uint32_t additional_permissions = 0;

#if !defined(OS_NACL)
  // Testing permissions. The testing flag implies all permissions since the
  // test plugin needs to test all interfaces.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting))
    additional_permissions |= ppapi::PERMISSION_ALL_BITS;
#endif

  return PpapiPermissions(base_perms | additional_permissions);
}

bool PpapiPermissions::HasPermission(Permission perm) const {
  // Check that "perm" is a power of two to make sure the caller didn't set
  // more than one permission bit. We may want to change how permissions are
  // represented in the future so don't want callers making assumptions about
  // bits.
  uint32_t perm_int = static_cast<uint32_t>(perm);
  if (!perm_int)
    return true;  // You always have "no permission".
  DCHECK((perm_int & (perm_int - 1)) == 0);
  return !!(permissions_ & perm_int);
}

}  // namespace ppapi
