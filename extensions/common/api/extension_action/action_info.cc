// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/extension_action/action_info.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

namespace {

constexpr char kEnabled[] = "enabled";
constexpr char kDisabled[] = "disabled";

// The manifest data container for the ActionInfos for BrowserActions and
// PageActions.
struct ActionInfoData : public Extension::ManifestData {
  explicit ActionInfoData(std::unique_ptr<ActionInfo> action_info);
  ~ActionInfoData() override;

  // The action associated with the BrowserAction.
  std::unique_ptr<ActionInfo> action_info;
};

ActionInfoData::ActionInfoData(std::unique_ptr<ActionInfo> info)
    : action_info(std::move(info)) {}

ActionInfoData::~ActionInfoData() = default;

}  // namespace

ActionInfo::ActionInfo(Type type) : type(type), synthesized(false) {
  switch (type) {
    case ActionInfo::Type::kPage:
      default_state = ActionInfo::DefaultState::kDisabled;
      break;
    case ActionInfo::Type::kBrowser:
    case ActionInfo::Type::kAction:
      default_state = ActionInfo::DefaultState::kEnabled;
      break;
  }
}

ActionInfo::ActionInfo(const ActionInfo& other) = default;

ActionInfo::~ActionInfo() = default;

// static
std::unique_ptr<ActionInfo> ActionInfo::Load(
    const Extension* extension,
    Type type,
    const base::Value::Dict& dict,
    std::vector<InstallWarning>* install_warnings,
    std::u16string* error) {
  auto result = std::make_unique<ActionInfo>(type);

  // Read the page action |default_icon| (optional).
  // The |default_icon| value can be either dictionary {icon size -> icon path}
  // or non empty string value.
  if (const base::Value* default_icon = dict.Find(keys::kActionDefaultIcon)) {
    std::string default_icon_str;
    if (default_icon->is_string())
      default_icon_str = default_icon->GetString();

    if (default_icon->is_dict()) {
      if (!manifest_handler_helpers::LoadIconsFromDictionary(
              default_icon->GetDict(), &result->default_icon, error)) {
        return nullptr;
      }
    } else if (default_icon->is_string() &&
               manifest_handler_helpers::NormalizeAndValidatePath(
                   &default_icon_str)) {
      // Choose the most optimistic (highest) icon density regardless of the
      // actual icon resolution, whatever that happens to be. Code elsewhere
      // knows how to scale down to 19.
      result->default_icon.Add(extension_misc::EXTENSION_ICON_GIGANTOR,
                               default_icon_str);
    } else {
      *error = errors::kInvalidActionDefaultIcon;
      return nullptr;
    }
  }

  // Read the page action title from |default_title| if present, |name| if not
  // (both optional).
  if (const base::Value* default_title = dict.Find(keys::kActionDefaultTitle)) {
    if (!default_title->is_string()) {
      *error = errors::kInvalidActionDefaultTitle;
      return nullptr;
    }
    result->default_title = default_title->GetString();
  }

  // Read the action's default popup (optional).
  if (const base::Value* default_popup = dict.Find(keys::kActionDefaultPopup)) {
    const std::string* url_str = default_popup->GetIfString();
    if (!url_str) {
      *error = errors::kInvalidActionDefaultPopup;
      return nullptr;
    }

    if (!url_str->empty()) {
      GURL popup_url = Extension::GetResourceURL(extension->url(), *url_str);

      if (!popup_url.is_valid()) {
        *error = errors::kInvalidActionDefaultPopup;
        return nullptr;
      }

      // Check popup is only for this extension.
      if (extension->origin().IsSameOriginWith(popup_url)) {
        result->default_popup_url = popup_url;
      } else {
        install_warnings->push_back(extensions::InstallWarning(
            extensions::manifest_errors::kInvalidExtensionOriginPopup,
            GetManifestKeyForActionType(type),
            extensions::manifest_keys::kActionDefaultPopup));
      }
    } else {
      // An empty string is treated as having no popup.
      DCHECK(result->default_popup_url.is_empty());
    }
  }

  if (const base::Value* default_state = dict.Find(keys::kActionDefaultState)) {
    // The default_state key is only valid for TYPE_ACTION; throw an error for
    // others.
    if (type != ActionInfo::Type::kAction) {
      *error = errors::kDefaultStateShouldNotBeSet;
      return nullptr;
    }

    if (!default_state->is_string() ||
        !(default_state->GetString() == kEnabled ||
          default_state->GetString() == kDisabled)) {
      *error = errors::kInvalidActionDefaultState;
      return nullptr;
    }
    result->default_state = default_state->GetString() == kEnabled
                                ? ActionInfo::DefaultState::kEnabled
                                : ActionInfo::DefaultState::kDisabled;
  }

  return result;
}

// static
const ActionInfo* ActionInfo::GetExtensionActionInfo(
    const Extension* extension) {
  const ActionInfoData* data =
      static_cast<ActionInfoData*>(extension->GetManifestData(keys::kAction));
  return data ? data->action_info.get() : nullptr;
}

// static
void ActionInfo::SetExtensionActionInfo(Extension* extension,
                                        std::unique_ptr<ActionInfo> info) {
  // Note: we store all actions (actions, browser actions, and page actions)
  // under the same key for simplicity because they are mutually exclusive,
  // and most callers shouldn't care about the type.
  extension->SetManifestData(keys::kAction,
                             std::make_unique<ActionInfoData>(std::move(info)));
}

// static
const char* ActionInfo::GetManifestKeyForActionType(ActionInfo::Type type) {
  const char* action_key = nullptr;
  switch (type) {
    case ActionInfo::Type::kBrowser:
      action_key = manifest_keys::kBrowserAction;
      break;
    case ActionInfo::Type::kPage:
      action_key = manifest_keys::kPageAction;
      break;
    case ActionInfo::Type::kAction:
      action_key = manifest_keys::kAction;
      break;
  }

  return action_key;
}

}  // namespace extensions
