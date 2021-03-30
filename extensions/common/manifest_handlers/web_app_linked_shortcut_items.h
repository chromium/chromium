// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_LINKED_SHORTCUT_ITEMS_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_LINKED_SHORTCUT_ITEMS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

class GURL;

namespace extensions {

// A structure to hold the parsed web app linked shortcut item data.
struct WebAppLinkedShortcutItems : public Extension::ManifestData {
  struct ShortcutItemInfo {
    struct IconInfo {
      IconInfo();
      ~IconInfo();

      GURL url;
      int size;
    };

    ShortcutItemInfo();
    ShortcutItemInfo(const ShortcutItemInfo& other);
    ~ShortcutItemInfo();

    std::u16string name;
    GURL url;
    std::vector<IconInfo> shortcut_item_icon_infos;
  };

  WebAppLinkedShortcutItems();
  WebAppLinkedShortcutItems(const WebAppLinkedShortcutItems& other);
  ~WebAppLinkedShortcutItems() override;

  static const WebAppLinkedShortcutItems& GetWebAppLinkedShortcutItems(
      const Extension* extension);

  std::vector<ShortcutItemInfo> shortcut_item_infos;
};

// Parses the "web_app_linked_shortcut_items" manifest key.
class WebAppLinkedShortcutItemsHandler : public ManifestHandler {
 public:
  WebAppLinkedShortcutItemsHandler();
  WebAppLinkedShortcutItemsHandler(const WebAppLinkedShortcutItemsHandler&) =
      delete;
  WebAppLinkedShortcutItemsHandler& operator=(
      const WebAppLinkedShortcutItemsHandler&) = delete;
  ~WebAppLinkedShortcutItemsHandler() override;

  // ManifestHandler:
  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  // ManifestHandler:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_LINKED_SHORTCUT_ITEMS_H_
