// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/web_app_linked_shortcut_items.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

bool ParseShortcutItemIconValue(
    const base::Value& value,
    WebAppLinkedShortcutItems::ShortcutItemInfo::IconInfo* icon_info,
    std::u16string* error) {
  const base::DictionaryValue* shortcut_item_icon_dict = nullptr;
  if (!value.GetAsDictionary(&shortcut_item_icon_dict)) {
    *error = base::UTF8ToUTF16(
        manifest_errors::kInvalidWebAppLinkedShortcutItemIcon);
    return false;
  }

  std::string icon_url;
  if (!shortcut_item_icon_dict->GetString(
          keys::kWebAppLinkedShortcutItemIconURL, &icon_url)) {
    *error = base::UTF8ToUTF16(
        manifest_errors::kInvalidWebAppLinkedShortcutItemIconUrl);
    return false;
  }

  icon_info->url = GURL(icon_url);
  if (!icon_info->url.is_valid()) {
    *error = base::UTF8ToUTF16(
        manifest_errors::kInvalidWebAppLinkedShortcutItemIconUrl);
    return false;
  }

  if (!shortcut_item_icon_dict->GetInteger(
          keys::kWebAppLinkedShortcutItemIconSize, &icon_info->size)) {
    *error = base::UTF8ToUTF16(
        manifest_errors::kInvalidWebAppLinkedShortcutItemIconSize);
    return false;
  }

  return true;
}

bool ParseShortcutItemValue(const base::Value& value,
                            WebAppLinkedShortcutItems::ShortcutItemInfo* info,
                            std::u16string* error) {
  const base::DictionaryValue* shortcut_item_dict = nullptr;
  if (!value.GetAsDictionary(&shortcut_item_dict)) {
    *error =
        base::UTF8ToUTF16(manifest_errors::kInvalidWebAppLinkedShortcutItem);
    return false;
  }

  if (!shortcut_item_dict->GetString(keys::kWebAppLinkedShortcutItemName,
                                     &info->name)) {
    *error = base::UTF8ToUTF16(
        manifest_errors::kInvalidWebAppLinkedShortcutItemName);
    return false;
  }

  std::string url_string;
  if (!shortcut_item_dict->GetString(keys::kWebAppLinkedShortcutItemURL,
                                     &url_string)) {
    *error =
        base::UTF8ToUTF16(manifest_errors::kInvalidWebAppLinkedShortcutItemUrl);
    return false;
  }

  info->url = GURL(url_string);
  if (!info->url.is_valid()) {
    *error =
        base::UTF8ToUTF16(manifest_errors::kInvalidWebAppLinkedShortcutItemUrl);
    return false;
  }

  const base::ListValue* shortcut_item_icons_value = nullptr;
  if (!shortcut_item_dict->GetList(keys::kWebAppLinkedShortcutItemIcons,
                                   &shortcut_item_icons_value)) {
    *error = base::UTF8ToUTF16(
        manifest_errors::kInvalidWebAppLinkedShortcutItemIcons);
    return false;
  }

  base::Value::ConstListView shortcut_item_icons_list =
      shortcut_item_icons_value->GetList();
  info->shortcut_item_icon_infos.reserve(shortcut_item_icons_list.size());

  for (const auto& shortcut_item_icon_value : shortcut_item_icons_list) {
    WebAppLinkedShortcutItems::ShortcutItemInfo::IconInfo icon_info;
    if (!ParseShortcutItemIconValue(shortcut_item_icon_value, &icon_info,
                                    error)) {
      return false;
    }
    info->shortcut_item_icon_infos.push_back(icon_info);
  }

  return true;
}

}  // namespace

WebAppLinkedShortcutItems::ShortcutItemInfo::IconInfo::IconInfo() = default;

WebAppLinkedShortcutItems::ShortcutItemInfo::IconInfo::~IconInfo() = default;

WebAppLinkedShortcutItems::ShortcutItemInfo::ShortcutItemInfo() = default;

WebAppLinkedShortcutItems::ShortcutItemInfo::ShortcutItemInfo(
    const WebAppLinkedShortcutItems::ShortcutItemInfo& other) = default;

WebAppLinkedShortcutItems::ShortcutItemInfo::~ShortcutItemInfo() = default;

WebAppLinkedShortcutItems::WebAppLinkedShortcutItems() = default;

WebAppLinkedShortcutItems::WebAppLinkedShortcutItems(
    const WebAppLinkedShortcutItems& other) = default;

WebAppLinkedShortcutItems::~WebAppLinkedShortcutItems() = default;

// static
const WebAppLinkedShortcutItems&
WebAppLinkedShortcutItems::GetWebAppLinkedShortcutItems(
    const Extension* extension) {
  WebAppLinkedShortcutItems* info = static_cast<WebAppLinkedShortcutItems*>(
      extension->GetManifestData(keys::kWebAppLinkedShortcutItems));
  if (info)
    return *info;

  static base::NoDestructor<WebAppLinkedShortcutItems>
      empty_web_app_linked_shortcut_items;
  return *empty_web_app_linked_shortcut_items;
}

WebAppLinkedShortcutItemsHandler::WebAppLinkedShortcutItemsHandler() = default;

WebAppLinkedShortcutItemsHandler::~WebAppLinkedShortcutItemsHandler() = default;

bool WebAppLinkedShortcutItemsHandler::Parse(Extension* extension,
                                             std::u16string* error) {
  // The "web_app_linked_shortcut_items" key is only available for Bookmark
  // Apps. Including it elsewhere results in an install warning, and the linked
  // shortcut items are not parsed.
  if (!extension->from_bookmark()) {
    extension->AddInstallWarning(InstallWarning(
        errors::kInvalidWebAppLinkedShortcutItemsNotBookmarkApp));
    return true;
  }

  auto web_app_linked_shortcut_items =
      std::make_unique<WebAppLinkedShortcutItems>();

  const base::Value* shortcut_items_value = nullptr;
  if (!extension->manifest()->GetList(keys::kWebAppLinkedShortcutItems,
                                      &shortcut_items_value)) {
    *error =
        base::UTF8ToUTF16(manifest_errors::kInvalidWebAppLinkedShortcutItems);
    return false;
  }

  base::Value::ConstListView shortcut_items_list =
      shortcut_items_value->GetList();
  web_app_linked_shortcut_items->shortcut_item_infos.reserve(
      shortcut_items_list.size());
  for (const auto& shortcut_item_value : shortcut_items_list) {
    WebAppLinkedShortcutItems::ShortcutItemInfo info;
    if (!ParseShortcutItemValue(shortcut_item_value, &info, error))
      return false;
    web_app_linked_shortcut_items->shortcut_item_infos.push_back(info);
  }
  extension->SetManifestData(keys::kWebAppLinkedShortcutItems,
                             std::move(web_app_linked_shortcut_items));
  return true;
}

base::span<const char* const> WebAppLinkedShortcutItemsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kWebAppLinkedShortcutItems};
  return kKeys;
}

}  // namespace extensions
