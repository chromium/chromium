// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_PERMISSIONS_H_
#define PPAPI_SHARED_IMPL_PPAPI_PERMISSIONS_H_

#include <stdint.h>

#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

enum Permission {
  // Placeholder/uninitialized permission.
  PERMISSION_NONE = 0,

  // Allows access to dev interfaces. These are experimental interfaces not
  // tied to any particular release channel.
  PERMISSION_DEV = 1 << 0,

  // Allows access to Browser-internal interfaces.
  PERMISSION_PRIVATE = 1 << 1,

  // Allows ability to bypass user-gesture checks for showing things like
  // file select dialogs.
  PERMISSION_BYPASS_USER_GESTURE = 1 << 2,

  // Testing-only interfaces.
  PERMISSION_TESTING = 1 << 3,

  // Flash-related interfaces.
  PERMISSION_FLASH = 1 << 4,

  // "Dev channel" interfaces. This is different than PERMISSION_DEV above;
  // these interfaces may only be used on Dev or Canary channel releases of
  // Chrome.
  PERMISSION_DEV_CHANNEL = 1 << 5,

  // PDF-related interfaces.
  PERMISSION_PDF = 1 << 6,

  // Socket APIs. Formerly part of public APIs.
  PERMISSION_SOCKET = 1 << 7,

  // NOTE: If you add stuff be sure to update PERMISSION_ALL_BITS.

  // Meta permission for for initializing plugins with permissions that have
  // historically been part of public APIs but are now covered by finer-grained
  // permissions.
  PERMISSION_DEFAULT = PERMISSION_SOCKET,

  // Meta permission for initializing plugins registered on the command line
  // that get all permissions.
  PERMISSION_ALL_BITS = PERMISSION_DEV | PERMISSION_PRIVATE |
                        PERMISSION_BYPASS_USER_GESTURE | PERMISSION_TESTING |
                        PERMISSION_FLASH | PERMISSION_DEV_CHANNEL |
                        PERMISSION_PDF | PERMISSION_SOCKET,
};

class PPAPI_SHARED_EXPORT PpapiPermissions {
 public:
  // Initializes the permissions struct with no permissions.
  PpapiPermissions();

  // Initializes with the given permissions bits set.
  explicit PpapiPermissions(uint32_t perms);

  ~PpapiPermissions();

  // Returns a permissions class with all features enabled. This is for testing
  // and manually registered plugins.
  static PpapiPermissions AllPermissions();

  // Returns the effective permissions given the "base" permissions granted
  // to the given plugin and the current command line flags, which may enable
  // more features.
  static PpapiPermissions GetForCommandLine(uint32_t base_perms);

  bool HasPermission(Permission perm) const;

  // Returns the internal permission bits. Use for serialization only.
  uint32_t GetBits() const { return permissions_; }

 private:
  uint32_t permissions_;

  // Note: Copy & assign supported.
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_PERMISSIONS_H_
