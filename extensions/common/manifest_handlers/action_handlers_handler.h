// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_ACTION_HANDLERS_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_ACTION_HANDLERS_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct ActionHandlersInfo : public Extension::ManifestData {
  // Returns true if the given |extension| has an action handler for
  // |action_type|.
  static bool HasActionHandler(const Extension* extension,
                               api::app_runtime::ActionType action_type);
  static bool HasLockScreenActionHandler(
      const Extension* extension,
      api::app_runtime::ActionType action_type);

  ActionHandlersInfo();
  ~ActionHandlersInfo() override;

  std::set<api::app_runtime::ActionType> action_handlers;
  std::set<api::app_runtime::ActionType> lock_screen_action_handlers;
};

// Parses the "action_handlers" manifest key.
class ActionHandlersHandler : public ManifestHandler {
 public:
  ActionHandlersHandler();

  ActionHandlersHandler(const ActionHandlersHandler&) = delete;
  ActionHandlersHandler& operator=(const ActionHandlersHandler&) = delete;

  ~ActionHandlersHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_ACTION_HANDLERS_HANDLER_H_
