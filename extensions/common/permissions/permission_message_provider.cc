// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_message_provider.h"

#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "extensions/common/extensions_client.h"

namespace extensions {

// static
const PermissionMessageProvider* PermissionMessageProvider::Get() {
  return &(ExtensionsClient::Get()->GetPermissionMessageProvider());
}

PermissionIDSet PermissionMessageProvider::GetManagementUIPermissionIDs(
    const PermissionSet& permissions,
    Manifest::Type extension_type) const {
  return PermissionIDSet();
}
}  // namespace extensions
