// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/extension_action_handler.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_constants.h"

// Adds `extensions::InstallWarning`s to `warnings` if the`default_popup` value
// for the action doesn't exist in the filesystem.
void SetWarningsForNonExistentDefaultPopup(
    const extensions::ActionInfo* action,
    const char* manifest_key,
    const extensions::Extension* extension,
    std::vector<extensions::InstallWarning>* warnings) {
  GURL default_popup_url = action->default_popup_url;
  if (default_popup_url.is_empty())
    return;

  GURL extension_base_url =
      extension->GetBaseURLFromExtensionId(extension->id());
  base::FilePath relative_path =
      extensions::file_util::ExtensionURLToRelativeFilePath(default_popup_url);
  base::FilePath resource_path =
      extension->GetResource(relative_path).GetFilePath();

  if (resource_path.empty() || !base::PathExists(resource_path)) {
    warnings->emplace_back(
        extensions::manifest_errors::kNonexistentDefaultPopup, manifest_key,
        extensions::manifest_keys::kActionDefaultPopup);
  }
}

namespace extensions {

ExtensionActionHandler::ExtensionActionHandler() = default;

ExtensionActionHandler::~ExtensionActionHandler() = default;

bool ExtensionActionHandler::Parse(Extension* extension,
                                   std::u16string* error) {
  const char* key = nullptr;
  const char* error_key = nullptr;
  ActionInfo::Type type = ActionInfo::Type::kAction;
  if (extension->manifest()->FindKey(manifest_keys::kAction)) {
    key = manifest_keys::kAction;
    error_key = manifest_errors::kInvalidAction;
    // type ACTION is correct.
  }

  if (extension->manifest()->FindKey(manifest_keys::kPageAction)) {
    if (key != nullptr) {
      // An extension can only have one action.
      *error = manifest_errors::kOneUISurfaceOnly;
      return false;
    }
    key = manifest_keys::kPageAction;
    error_key = manifest_errors::kInvalidPageAction;
    type = ActionInfo::Type::kPage;
  }

  if (extension->manifest()->FindKey(manifest_keys::kBrowserAction)) {
    if (key != nullptr) {
      // An extension can only have one action.
      *error = manifest_errors::kOneUISurfaceOnly;
      return false;
    }
    key = manifest_keys::kBrowserAction;
    error_key = manifest_errors::kInvalidBrowserAction;
    type = ActionInfo::Type::kBrowser;
  }

  if (key) {
    const base::Value::Dict* dict =
        extension->manifest()->available_values().FindDict(key);
    if (!dict) {
      *error = base::ASCIIToUTF16(error_key);
      return false;
    }

    std::vector<InstallWarning> install_warnings;
    std::unique_ptr<ActionInfo> action_info =
        ActionInfo::Load(extension, type, *dict, &install_warnings, error);
    extension->AddInstallWarnings(std::move(install_warnings));
    if (!action_info)
      return false;  // Failed to parse extension action definition.

    ActionInfo::SetExtensionActionInfo(extension, std::move(action_info));
  } else {  // No key, used for synthesizing an action for extensions with none.
    if (Manifest::IsComponentLocation(extension->location()))
      return true;  // Don't synthesize actions for component extensions.
    if (extension->was_installed_by_default())
      return true;  // Don't synthesize actions for default extensions.

    // Set an empty action. Manifest v2 extensions use page actions, whereas
    // manifest v3 use generic "actions". We use a page action (instead of a
    // browser action) for MV2 because the action should not be seen as enabled
    // on every page. We achieve the same in MV3 by adjusting the default
    // state to be disabled by default.
    type = extension->manifest_version() >= 3 ? ActionInfo::Type::kAction
                                              : ActionInfo::Type::kPage;
    auto action_info = std::make_unique<ActionInfo>(type);
    action_info->synthesized = true;
    if (type == ActionInfo::Type::kAction) {
      action_info->default_state = ActionInfo::DefaultState::kDisabled;
    }

    ActionInfo::SetExtensionActionInfo(extension, std::move(action_info));
  }

  return true;
}

bool ExtensionActionHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  const ActionInfo* action = ActionInfo::GetExtensionActionInfo(extension);

  if (!action)
    return true;

  const char* manifest_key =
      ActionInfo::GetManifestKeyForActionType(action->type);
  DCHECK(manifest_key);

  SetWarningsForNonExistentDefaultPopup(action, manifest_key, extension,
                                        warnings);

  // Empty default icon is valid.
  if (action->default_icon.empty())
    return true;

  // Analyze the icons for visibility using the default toolbar color, since
  // the majority of Chrome users don't modify their theme.
  return file_util::ValidateExtensionIconSet(action->default_icon, extension,
                                             manifest_key, error);
}

bool ExtensionActionHandler::AlwaysParseForType(Manifest::Type type) const {
  return type == Manifest::TYPE_EXTENSION || type == Manifest::TYPE_USER_SCRIPT;
}

bool ExtensionActionHandler::AlwaysValidateForType(Manifest::Type type) const {
  return type == Manifest::TYPE_EXTENSION || type == Manifest::TYPE_USER_SCRIPT;
}

base::span<const char* const> ExtensionActionHandler::Keys() const {
  static constexpr const char* kKeys[] = {
      manifest_keys::kPageAction,
      manifest_keys::kBrowserAction,
      manifest_keys::kAction,
  };
  return kKeys;
}

}  // namespace extensions
