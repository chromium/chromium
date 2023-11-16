// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/action_handlers_handler.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace app_runtime = api::app_runtime;
namespace errors = manifest_errors;
namespace keys = manifest_keys;

// static
bool ActionHandlersInfo::HasActionHandler(
    const Extension* extension,
    api::app_runtime::ActionType action_type) {
  ActionHandlersInfo* info = static_cast<ActionHandlersInfo*>(
      extension->GetManifestData(keys::kActionHandlers));
  return info && info->action_handlers.count(action_type) > 0;
}

bool ActionHandlersInfo::HasLockScreenActionHandler(
    const Extension* extension,
    api::app_runtime::ActionType action_type) {
  ActionHandlersInfo* info = static_cast<ActionHandlersInfo*>(
      extension->GetManifestData(keys::kActionHandlers));
  return info && info->lock_screen_action_handlers.count(action_type) > 0;
}

ActionHandlersInfo::ActionHandlersInfo() = default;

ActionHandlersInfo::~ActionHandlersInfo() = default;

ActionHandlersHandler::ActionHandlersHandler() = default;

ActionHandlersHandler::~ActionHandlersHandler() = default;

bool ActionHandlersHandler::Parse(Extension* extension, std::u16string* error) {
  const base::Value* entries = nullptr;
  if (!extension->manifest()->GetList(keys::kActionHandlers, &entries)) {
    *error = errors::kInvalidActionHandlersType;
    return false;
  }

  auto info = std::make_unique<ActionHandlersInfo>();
  for (const base::Value& wrapped_value : entries->GetList()) {
    std::string value;
    bool enabled_on_lock_screen = false;
    if (wrapped_value.is_dict()) {
      const base::Value::Dict& wrapped_dict = wrapped_value.GetDict();
      const std::string* action =
          wrapped_dict.FindString(keys::kActionHandlerActionKey);
      if (!action) {
        *error = errors::kInvalidActionHandlerDictionary;
        return false;
      }
      value = *action;
      std::optional<bool> enabled =
          wrapped_dict.FindBool(keys::kActionHandlerEnabledOnLockScreenKey);
      if (enabled) {
        enabled_on_lock_screen = *enabled;
      }
    } else if (wrapped_value.is_string()) {
      value = wrapped_value.GetString();
    } else {
      *error = errors::kInvalidActionHandlersType;
      return false;
    }

    app_runtime::ActionType action_type = app_runtime::ParseActionType(value);
    if (action_type == app_runtime::ActionType::kNone) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidActionHandlersActionType, value);
      return false;
    }

    if (info->action_handlers.count(action_type) > 0) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kDuplicateActionHandlerFound, value);
      return false;
    }
    info->action_handlers.insert(action_type);
    if (enabled_on_lock_screen)
      info->lock_screen_action_handlers.insert(action_type);
  }

  extension->SetManifestData(keys::kActionHandlers, std::move(info));
  return true;
}

base::span<const char* const> ActionHandlersHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kActionHandlers};
  return kKeys;
}

}  // namespace extensions
