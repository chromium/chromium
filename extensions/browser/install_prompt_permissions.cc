// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/install_prompt_permissions.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

InstallPromptPermissions::InstallPromptPermissions() = default;

InstallPromptPermissions::~InstallPromptPermissions() = default;

InstallPromptPermissions::InstallPromptPermissions(
    const InstallPromptPermissions& other) = default;

InstallPromptPermissions& InstallPromptPermissions::operator=(
    const InstallPromptPermissions& other) = default;

void InstallPromptPermissions::LoadFromPermissionSet(
    const PermissionSet* permissions_set,
    const Manifest::Type type) {
  const PermissionMessageProvider* message_provider =
      PermissionMessageProvider::Get();

  const PermissionMessages& permissions_messages =
      message_provider->GetPermissionMessages(
          message_provider->GetAllPermissionIDs(*permissions_set, type));

  AddPermissionMessages(permissions_messages);
}

void InstallPromptPermissions::AddPermissionMessages(
    const PermissionMessages& permissions_messages) {
  for (const PermissionMessage& msg : permissions_messages) {
    permissions.push_back(msg.message());
    std::u16string details_str;
    std::u16string visible_details_str;
    std::u16string collapsed_details_str;
    if (!msg.submessages().empty()) {
      std::vector<std::u16string> detail_lines_with_bullets;
      std::vector<std::u16string> visible_lines_with_bullets;
      std::vector<std::u16string> collapsed_lines_with_bullets;
      for (size_t i = 0; i < msg.submessages().size(); ++i) {
        std::u16string bulleted = u"• " + msg.submessages()[i];
        detail_lines_with_bullets.push_back(bulleted);
        if (i < kMaxVisibleDetails) {
          visible_lines_with_bullets.push_back(std::move(bulleted));
        } else {
          collapsed_lines_with_bullets.push_back(std::move(bulleted));
        }
      }

      details_str = base::JoinString(detail_lines_with_bullets, u"\n");
      visible_details_str = base::JoinString(visible_lines_with_bullets, u"\n");
      if (!collapsed_lines_with_bullets.empty()) {
        collapsed_details_str =
            base::JoinString(collapsed_lines_with_bullets, u"\n");
      }
    }
    details.push_back(details_str);
    visible_details.push_back(visible_details_str);
    collapsed_details.push_back(collapsed_details_str);
    is_showing_details.push_back(false);
  }
}

}  // namespace extensions
