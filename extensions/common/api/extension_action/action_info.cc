// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/extension_action/action_info.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/icons/extension_icon_variants.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"
#include "extensions/common/manifest_handlers/icon_variants_handler.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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

using extensions::diagnostics::icon_variants::Feature;
using extensions::diagnostics::icon_variants::Id;
using extensions::diagnostics::icon_variants::Severity;

// Returns the icon variants parsed from the `extension` manifest.
// Populates `error` if there are no icon variants.
ExtensionIconVariants GetIconVariants(const Extension& extension,
                                      const base::Value* value) {
  ExtensionIconVariants icon_variants;

  // Convert the input key into a list containing everything.
  if (!value->is_list()) {
    icon_variants.AddDiagnostic(Feature::kIconVariants,
                                Id::kIconVariantsKeyMustBeAList);
    return icon_variants;
  }

  icon_variants.Parse(extension, &value->GetList());

  // Verify `icon_variants`, e.g. that at least one `icon_variant` is valid.
  if (icon_variants.IsEmpty()) {
    icon_variants.AddDiagnostic(Feature::kIconVariants,
                                Id::kIconVariantsInvalid);
  }

  return icon_variants;
}

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

ActionInfo::ActionInfo(ActionInfo&& other) = default;

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

  // Read the action `default_icon` (optional).
  // The `default_icon` value can be either dictionary {icon size -> icon path}
  // or non empty string value.
  if (const base::Value* default_icon = dict.Find(keys::kActionDefaultIcon)) {
    if (default_icon->is_dict()) {
      std::vector<std::string> warnings;
      if (!manifest_handler_helpers::LoadIconsFromDictionary(
              *extension, default_icon->GetDict(), &result->default_icon, error,
              &warnings)) {
        return nullptr;
      }
      if (!warnings.empty()) {
        install_warnings->reserve(install_warnings->size() + warnings.size());
        std::string manifest_key = GetManifestKeyForActionType(type);
        for (const auto& warning : warnings) {
          install_warnings->emplace_back(warning, manifest_key,
                                         keys::kActionDefaultIcon);
        }
      }
    } else if (default_icon->is_string()) {
      ExtensionResource default_icon_resource =
          extension->GetResource(default_icon->GetString());
      if (default_icon_resource.empty()) {
        *error = errors::kInvalidActionDefaultIcon;
        return nullptr;
      }

      if (manifest_handler_helpers::IsIconMimeTypeValid(
              default_icon_resource.relative_path())) {
        // Choose the most optimistic (highest) icon density regardless of the
        // actual icon resolution, whatever that happens to be. Code elsewhere
        // knows how to scale down to 19.
        result->default_icon.Add(
            extension_misc::EXTENSION_ICON_GIGANTOR,
            default_icon_resource.relative_path().AsUTF8Unsafe());
      } else {
        // Issue a warning and ignore this file. This is a warning and not a
        // hard-error to preserve both backwards compatibility and potential
        // future-compatibility if mime types change.
        install_warnings->emplace_back(
            errors::kInvalidActionDefaultIconMimeType,
            GetManifestKeyForActionType(type), keys::kActionDefaultIcon);
      }
    } else {
      *error = errors::kInvalidActionDefaultIcon;
      return nullptr;
    }
  }

  // Read the action title from `default_title` if present, `name` if not
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

    if (*url_str == "index.html/") {
      // TODO(crbug.com/427225438): Warn and treat as having no popup. This
      // special case is here for compatibility with existing extensions which
      // use this invalid entry and then set another popup URL at runtime via
      // the relevant API. Remove this special case in the future.
      install_warnings->emplace_back(
          errors::kActionDefaultPopupInvalidCompatValue,
          GetManifestKeyForActionType(type), keys::kActionDefaultPopup);
      DCHECK(result->default_popup_url.is_empty());
    } else if (!url_str->empty()) {
      GURL popup_url = extension->GetResourceURL(*url_str);
      if (!popup_url.is_valid()) {
        *error = errors::kInvalidActionDefaultPopup;
        return nullptr;
      }

      result->default_popup_url = popup_url;
    } else {
      // An empty string is treated as having no popup.
      DCHECK(result->default_popup_url.is_empty());
    }
  }

  if (const base::Value* default_state = dict.Find(keys::kActionDefaultState)) {
    // The `default_state` key is only valid for TYPE_ACTION; throw an error for
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

  bool supports_icon_variants =
      IconVariantsInfo::SupportsIconVariants(*extension);
  const base::Value* icon_variants_value = dict.Find(keys::kIconVariants);
  if (icon_variants_value) {
    if (!supports_icon_variants) {
      auto diagnostic = extensions::diagnostics::icon_variants::GetDiagnostic(
          Feature::kIconVariants, Id::kIconVariantsNotEnabled);
      install_warnings->emplace_back(diagnostic.message);
    } else {
      ExtensionIconVariants icon_variants =
          GetIconVariants(*extension, icon_variants_value);

      // Add any install warnings, handle errors, and then clear out
      // diagnostics.
      auto& diagnostics = icon_variants.get_diagnostics();
      for (const auto& diagnostic : diagnostics) {
        switch (diagnostic.severity) {
          case Severity::kWarning:
            // Add an install warning.
            install_warnings->emplace_back(diagnostic.message);
            break;
          case Severity::kError:
            // If any error exists, do not load the extension.
            *error = base::UTF8ToUTF16(diagnostic.message);
            return nullptr;
          default:
            NOTREACHED();
        }

        install_warnings->emplace_back(diagnostic.message);
      }

      diagnostics.clear();
      result->icon_variants = std::move(icon_variants);
    }
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
