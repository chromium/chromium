// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_PROMPT_PERMISSIONS_H_
#define EXTENSIONS_BROWSER_INSTALL_PROMPT_PERMISSIONS_H_

#include <string>
#include <vector>

#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class PermissionSet;

struct InstallPromptPermissions {
  InstallPromptPermissions();
  ~InstallPromptPermissions();
  InstallPromptPermissions(const InstallPromptPermissions&);
  InstallPromptPermissions& operator=(const InstallPromptPermissions&);

  void LoadFromPermissionSet(const extensions::PermissionSet* permissions_set,
                             extensions::Manifest::Type type);

  void AddPermissionMessages(
      const extensions::PermissionMessages& permissions_messages);

  std::vector<std::u16string> permissions;
  std::vector<std::u16string> details;
  std::vector<bool> is_showing_details;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_PROMPT_PERMISSIONS_H_
