// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_EXTENSION_ACTION_ACTION_INFO_H_
#define EXTENSIONS_COMMON_API_EXTENSION_ACTION_ACTION_INFO_H_

#include <memory>
#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "url/gurl.h"

namespace extensions {

class Extension;

struct ActionInfo {
  // The types of extension actions.
  enum class Type {
    kAction,
    kBrowser,
    kPage,
  };

  enum class DefaultState {
    kEnabled,
    kDisabled,
  };

  explicit ActionInfo(Type type);
  ActionInfo(const ActionInfo& other);
  ~ActionInfo();

  // Loads an ActionInfo from the given Dict. Populating
  // `install_warnings` if issues are encountered when parsing the manifest.
  static std::unique_ptr<ActionInfo> Load(
      const Extension* extension,
      Type type,
      const base::Value::Dict& dict,
      std::vector<InstallWarning>* install_warnings,
      std::u16string* error);

  // TODO(jlulejian): Rather than continue to grow this list of static helper
  // methods, move them to a action_helper.h class similar to
  // chrome/browser/extensions/settings_api_helpers.h

  // Returns any action associated with the extension, whether it's specified
  // under the "page_action", "browser_action", or "action" key.
  static const ActionInfo* GetExtensionActionInfo(const Extension* extension);

  // Retrieves the manifest key for the given action |type|.
  static const char* GetManifestKeyForActionType(ActionInfo::Type type);

  // Sets the extension's action.
  static void SetExtensionActionInfo(Extension* extension,
                                     std::unique_ptr<ActionInfo> info);

  // The key this action corresponds to. NOTE: You should only use this if you
  // care about the actual manifest key. Use the other members (like
  // |default_state| for querying general info.
  const Type type;

  // Empty implies the key wasn't present.
  ExtensionIconSet default_icon;
  std::string default_title;
  GURL default_popup_url;

  // Specifies if the action applies to all web pages ("enabled") or
  // only specific pages ("disabled"). Only applies to the "action" key.
  DefaultState default_state;
  // Whether or not this action was synthesized to force visibility.
  bool synthesized;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_EXTENSION_ACTION_ACTION_INFO_H_
