// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_SHORTCUT_ICONS_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_SHORTCUT_ICONS_HANDLER_H_

#include <map>
#include <string>

#include "base/containers/span.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct WebAppShortcutIconsInfo : public Extension::ManifestData {
  WebAppShortcutIconsInfo();
  ~WebAppShortcutIconsInfo() override;

  // Return the shortcut icon map for the given |extension|.
  static const std::map<int, ExtensionIconSet>& GetShortcutIcons(
      const Extension* extension);

  // Get an extension icon as a resource or URL.
  static ExtensionResource GetIconResource(
      const Extension* extension,
      int shortcut_index,
      int size_in_px,
      ExtensionIconSet::MatchType match_type);

  // shortcut icons for the extension as a shortcut index to icons mapping.
  std::map<int, ExtensionIconSet> shortcut_icons_map;
};

// Parses the "web_app_shortcut_icons" manifest key.
class WebAppShortcutIconsHandler : public ManifestHandler {
 public:
  WebAppShortcutIconsHandler();
  ~WebAppShortcutIconsHandler() override;

  // ManifestHandler:
  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  // ManifestHandler:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_SHORTCUT_ICONS_HANDLER_H_
