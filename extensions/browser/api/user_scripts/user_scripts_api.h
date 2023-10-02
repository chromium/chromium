// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_USER_SCRIPTS_USER_SCRIPTS_API_H_
#define EXTENSIONS_BROWSER_API_USER_SCRIPTS_USER_SCRIPTS_API_H_

#include "extensions/browser/api/scripting/scripting_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/common/api/user_scripts.h"

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

class UserScriptsGetScriptsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("userScripts.getScripts", USERSCRIPTS_GETSCRIPTS)

  UserScriptsGetScriptsFunction() = default;
  UserScriptsGetScriptsFunction(const UserScriptsRegisterFunction&) = delete;
  const UserScriptsGetScriptsFunction& operator=(
      const UserScriptsGetScriptsFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~UserScriptsGetScriptsFunction() override = default;
};

class UserScriptsUnregisterFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("userScripts.unregister", USERSCRIPTS_UNREGISTER)

  UserScriptsUnregisterFunction() = default;
  UserScriptsUnregisterFunction(const UserScriptsUnregisterFunction&) = delete;
  const UserScriptsUnregisterFunction& operator=(
      const UserScriptsUnregisterFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~UserScriptsUnregisterFunction() override = default;

  // Called when user scripts have been unregistered..
  void OnUserScriptsUnregistered(const absl::optional<std::string>& error);
};

class UserScriptsUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("userScripts.update", USERSCRIPTS_UPDATE)

  UserScriptsUpdateFunction() = default;
  UserScriptsUpdateFunction(const UserScriptsUpdateFunction&) = delete;
  const UserScriptsUpdateFunction& operator=(const UserScriptsUpdateFunction&) =
      delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~UserScriptsUpdateFunction() override = default;

  // Returns a UserScript object by updating the `original_script` with the
  // `new_script` given delta. If the updated script cannot be parsed, populates
  // `parse_error` and returns nullptr.
  // Note: While `definition_index` is not used, we have it as a parameter to
  // match other ApplyUpdate() callbacks.
  std::unique_ptr<UserScript> ApplyUpdate(
      api::user_scripts::RegisteredUserScript& new_script,
      api::user_scripts::RegisteredUserScript& original_script,
      int definition_index,
      std::u16string* parse_error);

  // Called when user script files have been validated.
  void OnUserScriptFilesValidated(scripting::ValidateScriptsResult result);

  // Called when user scripts have been updated..
  void OnUserScriptsUpdated(const absl::optional<std::string>& error);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_USER_SCRIPTS_USER_SCRIPTS_API_H_
