// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/web_app_shortcut_icons_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

WebAppShortcutIconsInfo::WebAppShortcutIconsInfo() = default;

WebAppShortcutIconsInfo::~WebAppShortcutIconsInfo() = default;

// static
const std::map<int, ExtensionIconSet>&
WebAppShortcutIconsInfo::GetShortcutIcons(const Extension* extension) {
  DCHECK(extension);
  WebAppShortcutIconsInfo* info = static_cast<WebAppShortcutIconsInfo*>(
      extension->GetManifestData(keys::kWebAppShortcutIcons));
  if (info)
    return info->shortcut_icons_map;

  static base::NoDestructor<std::map<int, ExtensionIconSet>>
      empty_shortcut_icons_map;
  return *empty_shortcut_icons_map;
}

// static
ExtensionResource WebAppShortcutIconsInfo::GetIconResource(
    const Extension* extension,
    int shortcut_index,
    int size_in_px,
    ExtensionIconSet::MatchType match_type) {
  const std::string& path = GetShortcutIcons(extension)
                                .at(shortcut_index)
                                .Get(size_in_px, match_type);
  return path.empty() ? ExtensionResource() : extension->GetResource(path);
}

WebAppShortcutIconsHandler::WebAppShortcutIconsHandler() = default;

WebAppShortcutIconsHandler::~WebAppShortcutIconsHandler() = default;

bool WebAppShortcutIconsHandler::Parse(Extension* extension,
                                       std::u16string* error) {
  // The "web_app_shortcut_icons" key is only available for Bookmark Apps.
  // Including it elsewhere results in an install warning, and the shortcut
  // icons are not parsed.
  if (!extension->from_bookmark()) {
    extension->AddInstallWarning(
        InstallWarning(errors::kInvalidWebAppShortcutIconsNotBookmarkApp));
    return true;
  }

  auto shortcut_icons_info = std::make_unique<WebAppShortcutIconsInfo>();

  const base::Value* shortcut_icons_val = nullptr;
  if (!extension->manifest()->GetDictionary(keys::kWebAppShortcutIcons,
                                            &shortcut_icons_val)) {
    *error = base::ASCIIToUTF16(errors::kInvalidWebAppShortcutIcons);
    return false;
  }

  int shortcut_index = -1;
  for (auto entry : shortcut_icons_val->DictItems()) {
    ++shortcut_index;
    const base::DictionaryValue* shortcut_item_icons = nullptr;
    if (!entry.second.GetAsDictionary(&shortcut_item_icons)) {
      *error = base::ASCIIToUTF16(errors::kInvalidWebAppShortcutItemIcons);
      return false;
    }

    if (!manifest_handler_helpers::LoadIconsFromDictionary(
            shortcut_item_icons,
            &shortcut_icons_info->shortcut_icons_map[shortcut_index], error)) {
      return false;
    }
  }

  extension->SetManifestData(keys::kWebAppShortcutIcons,
                             std::move(shortcut_icons_info));
  return true;
}

base::span<const char* const> WebAppShortcutIconsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kWebAppShortcutIcons};
  return kKeys;
}

}  // namespace extensions
