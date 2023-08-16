// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_USER_SCRIPTS_USER_SCRIPTS_API_H_
#define EXTENSIONS_BROWSER_API_USER_SCRIPTS_USER_SCRIPTS_API_H_

#include "extensions/browser/api/scripting/scripting_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class UserScriptsRegisterFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("userScripts.register", USERSCRIPTS_REGISTER)

  UserScriptsRegisterFunction() = default;
  UserScriptsRegisterFunction(const UserScriptsRegisterFunction&) = delete;
  const UserScriptsRegisterFunction& operator=(
      const UserScriptsRegisterFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~UserScriptsRegisterFunction() override = default;

  // Called when user script files have been validated.
  void OnUserScriptFilesValidated(scripting::ValidateScriptsResult result);

  // Called when user scripts have been registered.
  void OnUserScriptsRegistered(const absl::optional<std::string>& error);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_USER_SCRIPTS_USER_SCRIPTS_API_H_
