// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_PERMISSION_MESSAGE_PROVIDER_H_
#define EXTENSIONS_TEST_TEST_PERMISSION_MESSAGE_PROVIDER_H_

#include "base/macros.h"
#include "extensions/common/permissions/permission_message_provider.h"

namespace extensions {

class TestPermissionMessageProvider : public PermissionMessageProvider {
 public:
  TestPermissionMessageProvider();
  ~TestPermissionMessageProvider() override;

 private:
  PermissionMessages GetPermissionMessages(
      const PermissionIDSet& permissions) const override;
  PermissionMessages GetPowerfulPermissionMessages(
      const PermissionIDSet& permissions) const override;
  bool IsPrivilegeIncrease(const PermissionSet& granted_permissions,
                           const PermissionSet& requested_permissions,
                           Manifest::Type extension_type) const override;
  PermissionIDSet GetAllPermissionIDs(
      const PermissionSet& permissions,
      Manifest::Type extension_type) const override;

  DISALLOW_COPY_AND_ASSIGN(TestPermissionMessageProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_PERMISSION_MESSAGE_PROVIDER_H_
