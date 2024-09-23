// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/user_scripts/user_scripts_api.h"

#include <memory>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/scripting_constants.h"
#include "extensions/browser/scripting_utils.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/browser/user_script_world_configuration_manager.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/api/scripts_internal/script_serialization.h"
#include "extensions/common/api/user_scripts.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/execution_world.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "extensions/common/utils/content_script_utils.h"

namespace extensions {

namespace {

constexpr char kEmptySourceError[] =
    "User script with ID '*' must specify at least one js source.";
constexpr char kInvalidSourceError[] =
    "User script with ID '*' must specify exactly one of 'code' or 'file' as a "
    "js source.";
constexpr char kMatchesMissingError[] =
    "User script with ID '*' must specify 'matches'.";

// Returns true if the given `world_id` is valid from the API perspective.
// If invalid, populates `error_out`.
bool IsValidWorldId(const std::optional<std::string>& world_id,
                    std::string* error_out) {
  if (!world_id) {
    // Omitting world ID is valid.
    return true;
  }

  if (world_id->empty()) {
    *error_out = "If specified, `worldId` must be non-empty.";
    return false;
  }

  if (world_id->at(0) == '_') {
    *error_out = "World IDs beginning with '_' are reserved.";
    return false;
  }

  static constexpr size_t kMaxWorldIdLength = 256;
  if (world_id->length() > kMaxWorldIdLength) {
    *error_out = "World IDs must be at most 256 characters.";
    return false;
  }

  // Valid world ID!
  return true;
}

api::scripts_internal::SerializedUserScript
ConvertRegisteredUserScriptToSerializedUserScript(
    api::user_scripts::RegisteredUserScript user_script) {
  auto user_script_sources_to_serialized_sources =
      [](std::vector<api::user_scripts::ScriptSource> sources) {
        std::vector<api::scripts_internal::ScriptSource> result;
        result.reserve(sources.size());
        for (auto& source : sources) {
          api::scripts_internal::ScriptSource converted_source;
          converted_source.code = std::move(source.code);
          converted_source.file = std::move(source.file);
          result.push_back(std::move(converted_source));
        }
        return result;
      };

  auto convert_execution_world = [](api::user_scripts::ExecutionWorld world) {
    switch (world) {
      // Execution world defaults to `kUserScript` when it's not provided.
      case api::user_scripts::ExecutionWorld::kNone:
      case api::user_scripts::ExecutionWorld::kUserScript:
        return api::extension_types::ExecutionWorld::kUserScript;
      case api::user_scripts::ExecutionWorld::kMain:
        return api::extension_types::ExecutionWorld::kMain;
    }
  };

  api::scripts_internal::SerializedUserScript serialized_script;
  serialized_script.source = api::scripts_internal::Source::kDynamicUserScript;

  serialized_script.all_frames = user_script.all_frames;
  serialized_script.exclude_matches = std::move(user_script.exclude_matches);
  // Note: IDs have already been prefixed appropriately.
  serialized_script.id = std::move(user_script.id);
  serialized_script.include_globs = std::move(user_script.include_globs);
  serialized_script.exclude_globs = std::move(user_script.exclude_globs);
  serialized_script.js =
      user_script_sources_to_serialized_sources(std::move(user_script.js));
  serialized_script.matches = std::move(*user_script.matches);
  serialized_script.run_at = std::move(user_script.run_at);
  serialized_script.world = convert_execution_world(user_script.world);

  if (base::FeatureList::IsEnabled(
          extensions_features::kApiUserScriptsMultipleWorlds)) {
    serialized_script.world_id = std::move(user_script.world_id);
  }

  return serialized_script;
}

std::unique_ptr<UserScript> ParseUserScript(
    const Extension& extension,
    api::user_scripts::RegisteredUserScript user_script,
    bool allowed_in_incognito,
    std::u16string* error) {
  // Custom validation unique to user scripts.
  // `matches` must be specified for newly-registered scripts, despite being
  // an optional argument.
  if (!user_script.matches) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        kMatchesMissingError,
        UserScript::TrimPrefixFromScriptID(user_script.id));
    return nullptr;
  }

  // `js` must not be empty.
  if (user_script.js.empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        kEmptySourceError, UserScript::TrimPrefixFromScriptID(user_script.id));
    return nullptr;
  }

  for (const api::user_scripts::ScriptSource& source : user_script.js) {
    if ((source.code && source.file) || (!source.code && !source.file)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          kInvalidSourceError,
          UserScript::TrimPrefixFromScriptID(user_script.id));
      return nullptr;
    }
  }

  std::string utf8_error;
  if (!IsValidWorldId(user_script.world_id, &utf8_error)) {
    *error = base::UTF8ToUTF16(utf8_error);
    return nullptr;
  }

  // After this, we can just convert to our internal type and rely on our
  // typical parsing to a `UserScript`.
  api::scripts_internal::SerializedUserScript serialized_script =
      ConvertRegisteredUserScriptToSerializedUserScript(std::move(user_script));

  return script_serialization::ParseSerializedUserScript(
      serialized_script, extension, allowed_in_incognito, error);
}

// Converts a UserScript object to a api::user_scripts::RegisteredUserScript
// object, used for getScripts.
api::user_scripts::RegisteredUserScript CreateRegisteredUserScriptInfo(
    const UserScript& script) {
  CHECK_EQ(UserScript::Source::kDynamicUserScript, script.GetSource());

  // To convert a `UserScript`, we first go through our script_internal
  // serialization; this allows us to do simple conversions and avoid any
  // complex logic.
  api::scripts_internal::SerializedUserScript serialized_script =
      script_serialization::SerializeUserScript(script);

  auto convert_serialized_script_sources =
      [](std::vector<api::scripts_internal::ScriptSource> sources) {
        std::vector<api::user_scripts::ScriptSource> converted;
        converted.reserve(sources.size());
        for (auto& source : sources) {
          api::user_scripts::ScriptSource converted_source;
          converted_source.code = std::move(source.code);
          converted_source.file = std::move(source.file);
          converted.push_back(std::move(converted_source));
        }
        return converted;
      };

  auto convert_execution_world =
      [](api::extension_types::ExecutionWorld world) {
        switch (world) {
          case api::extension_types::ExecutionWorld::kNone:
            NOTREACHED()
                << "Execution world should always be present in serialization.";
          case api::extension_types::ExecutionWorld::kIsolated:
            NOTREACHED() << "ISOLATED worlds are not supported in this API.";
          case api::extension_types::ExecutionWorld::kUserScript:
            return api::user_scripts::ExecutionWorld::kUserScript;
          case api::extension_types::ExecutionWorld::kMain:
            return api::user_scripts::ExecutionWorld::kMain;
        }
      };

  api::user_scripts::RegisteredUserScript result;
  result.all_frames = serialized_script.all_frames;
  result.exclude_matches = std::move(serialized_script.exclude_matches);
  result.id = std::move(serialized_script.id);
  result.include_globs = std::move(serialized_script.include_globs);
  result.exclude_globs = std::move(serialized_script.exclude_globs);
  if (serialized_script.js) {
    result.js =
        convert_serialized_script_sources(std::move(*serialized_script.js));
  }
  result.matches = std::move(serialized_script.matches);
  result.run_at = serialized_script.run_at;
  result.world = convert_execution_world(serialized_script.world);
  result.world_id = std::move(serialized_script.world_id);

  return result;
}

}  // namespace

ExtensionFunction::ResponseAction UserScriptsRegisterFunction::Run() {
  std::optional<api::user_scripts::Register::Params> params(
      api::user_scripts::Register::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(extension());

  std::vector<api::user_scripts::RegisteredUserScript>& scripts =
      params->scripts;
  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());

  // Create script ids for dynamic user scripts.
  std::string error;
  std::set<std::string> existing_script_ids =
      loader->GetDynamicScriptIDs(UserScript::Source::kDynamicUserScript);
  std::set<std::string> new_script_ids = scripting::CreateDynamicScriptIds(
      scripts, UserScript::Source::kDynamicUserScript, existing_script_ids,
      &error);

  if (!error.empty()) {
    CHECK(new_script_ids.empty());
    return RespondNow(Error(std::move(error)));
  }

  // Parse user scripts.
  UserScriptList parsed_scripts;
  parsed_scripts.reserve(scripts.size());
  std::u16string parse_error;

  bool allowed_in_incognito = scripting::ScriptsShouldBeAllowedInIncognito(
      extension()->id(), browser_context());

  for (auto& script : scripts) {
    std::unique_ptr<UserScript> user_script = ParseUserScript(
        *extension(), std::move(script), allowed_in_incognito, &parse_error);
    if (!user_script) {
      return RespondNow(Error(base::UTF16ToASCII(parse_error)));
    }

    parsed_scripts.push_back(std::move(user_script));
  }
  scripts.clear();  // The contents of `scripts` have been std::move()d.

  // Add new script IDs now in case another call with the same script IDs is
  // made immediately following this one.
  loader->AddPendingDynamicScriptIDs(std::move(new_script_ids));

  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&scripting::ValidateParsedScriptsOnFileThread,
                     script_parsing::GetSymlinkPolicy(extension()),
                     std::move(parsed_scripts)),
      base::BindOnce(&UserScriptsRegisterFunction::OnUserScriptFilesValidated,
                     this));

  // Balanced in `OnUserScriptFilesValidated()` or `OnUserScriptsRegistered()`.
  AddRef();
  return RespondLater();
}

void UserScriptsRegisterFunction::OnUserScriptFilesValidated(
    scripting::ValidateScriptsResult result) {
  // We cannot proceed if the `browser_context` is not valid as the
  // `ExtensionSystem` will not exist.
  if (!browser_context()) {
    Release();  // Matches the `AddRef()` in `Run()`.
    return;
  }

  // We cannot proceed if the extension is uninstalled or unloaded in the middle
  // of validating its script files.
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  if (!extension() ||
      !registry->enabled_extensions().Contains(extension_id())) {
    // Note: a Respond() is not needed if the system is shutting down or if the
    // extension is no longer enabled.
    Release();  // Matches the `AddRef()` in `Run()`.
    return;
  }

  auto error = std::move(result.second);
  auto scripts = std::move(result.first);

  std::set<std::string> script_ids;
  for (const auto& script : scripts) {
    script_ids.insert(script->id());
  }
  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());

  if (error.has_value()) {
    loader->RemovePendingDynamicScriptIDs(std::move(script_ids));
    Respond(Error(std::move(*error)));
    Release();  // Matches the `AddRef()` in `Run()`.
    return;
  }

  // User scripts are always persisted across sessions.
  loader->AddDynamicScripts(
      std::move(scripts), /*persistent_script_ids=*/std::move(script_ids),
      base::BindOnce(&UserScriptsRegisterFunction::OnUserScriptsRegistered,
                     this));
}

void UserScriptsRegisterFunction::OnUserScriptsRegistered(
    const std::optional<std::string>& error) {
  if (error.has_value()) {
    Respond(Error(std::move(*error)));
  } else {
    Respond(NoArguments());
  }
  Release();  // Matches the `AddRef()` in `Run()`.
}

ExtensionFunction::ResponseAction UserScriptsGetScriptsFunction::Run() {
  std::optional<api::user_scripts::GetScripts::Params> params =
      api::user_scripts::GetScripts::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::optional<api::user_scripts::UserScriptFilter>& filter = params->filter;
  std::set<std::string> id_filter;
  if (filter && filter->ids) {
    id_filter.insert(std::make_move_iterator(filter->ids->begin()),
                     std::make_move_iterator(filter->ids->end()));
  }

  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());
  const UserScriptList& dynamic_scripts = loader->GetLoadedDynamicScripts();

  std::vector<api::user_scripts::RegisteredUserScript> registered_user_scripts;
  for (const std::unique_ptr<UserScript>& script : dynamic_scripts) {
    if (script->GetSource() != UserScript::Source::kDynamicUserScript) {
      continue;
    }

    std::string id_without_prefix = script->GetIDWithoutPrefix();
    if (filter && filter->ids &&
        !base::Contains(id_filter, id_without_prefix)) {
      continue;
    }

    auto user_script = CreateRegisteredUserScriptInfo(*script);
    // Remove the internally used prefix from the `script`'s ID before
    // returning.
    user_script.id = id_without_prefix;
    registered_user_scripts.push_back(std::move(user_script));
  }

  return RespondNow(ArgumentList(
      api::user_scripts::GetScripts::Results::Create(registered_user_scripts)));
}

ExtensionFunction::ResponseAction UserScriptsUnregisterFunction::Run() {
  std::optional<api::user_scripts::Unregister::Params> params(
      api::user_scripts::Unregister::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(extension());

  std::optional<api::user_scripts::UserScriptFilter>& filter = params->filter;
  std::optional<std::vector<std::string>> ids = std::nullopt;
  if (filter && filter->ids) {
    ids = filter->ids;
  }

  std::string error;
  bool removal_triggered = scripting::RemoveScripts(
      ids, UserScript::Source::kDynamicUserScript, browser_context(),
      extension()->id(),
      base::BindOnce(&UserScriptsUnregisterFunction::OnUserScriptsUnregistered,
                     this),
      &error);

  if (!removal_triggered) {
    CHECK(!error.empty());
    return RespondNow(Error(std::move(error)));
  }

  return RespondLater();
}

void UserScriptsUnregisterFunction::OnUserScriptsUnregistered(
    const std::optional<std::string>& error) {
  if (error.has_value()) {
    Respond(Error(std::move(*error)));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction UserScriptsUpdateFunction::Run() {
  std::optional<api::user_scripts::Update::Params> params(
      api::user_scripts::Update::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(extension());

  std::vector<api::user_scripts::RegisteredUserScript>& scripts_to_update =
      params->scripts;
  std::string error;

  // Add the prefix for dynamic user scripts onto the IDs of all `scripts`
  // before continuing.
  std::set<std::string> ids_to_update = scripting::CreateDynamicScriptIds(
      scripts_to_update, UserScript::Source::kDynamicUserScript,
      /*existing_script_ids=*/std::set<std::string>(), &error);

  if (!error.empty()) {
    CHECK(ids_to_update.empty());
    return RespondNow(Error(std::move(error)));
  }

  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());

  UserScriptList parsed_scripts = scripting::UpdateScripts(
      scripts_to_update, UserScript::Source::kDynamicUserScript, *loader,
      base::BindRepeating(&CreateRegisteredUserScriptInfo),
      base::BindRepeating(&UserScriptsUpdateFunction::ApplyUpdate, this),
      &error);

  if (!error.empty()) {
    CHECK(parsed_scripts.empty());
    return RespondNow(Error(std::move(error)));
  }

  // Add new script IDs now in case another call with the same script IDs is
  // made immediately following this one.
  loader->AddPendingDynamicScriptIDs(std::move(ids_to_update));

  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&scripting::ValidateParsedScriptsOnFileThread,
                     script_parsing::GetSymlinkPolicy(extension()),
                     std::move(parsed_scripts)),
      base::BindOnce(&UserScriptsUpdateFunction::OnUserScriptFilesValidated,
                     this));

  // Balanced in `OnUserScriptFilesValidated()`.
  AddRef();
  return RespondLater();
}

std::unique_ptr<UserScript> UserScriptsUpdateFunction::ApplyUpdate(
    api::user_scripts::RegisteredUserScript& new_script,
    api::user_scripts::RegisteredUserScript& original_script,
    std::u16string* parse_error) {
  if (new_script.run_at != api::extension_types::RunAt::kNone) {
    original_script.run_at = new_script.run_at;
  }

  if (new_script.all_frames) {
    original_script.all_frames = *new_script.all_frames;
  }

  if (new_script.matches) {
    original_script.matches = std::move(new_script.matches);
  }

  if (new_script.exclude_matches) {
    original_script.exclude_matches = std::move(new_script.exclude_matches);
  }

  if (!new_script.js.empty()) {
    original_script.js = std::move(new_script.js);
  }

  if (new_script.world != api::user_scripts::ExecutionWorld::kNone) {
    original_script.world = new_script.world;
  }

  if (new_script.world_id) {
    original_script.world_id = std::move(new_script.world_id);
  }

  // Note: for the update application, we disregard allowed_in_incognito.
  // We'll set it on the resulting scripts.
  constexpr bool kAllowedInIncognito = false;

  std::unique_ptr<UserScript> parsed_script =
      ParseUserScript(*extension(), std::move(original_script),
                      kAllowedInIncognito, parse_error);
  return parsed_script;
}

void UserScriptsUpdateFunction::OnUserScriptFilesValidated(
    scripting::ValidateScriptsResult result) {
  // We cannot proceed if the `browser_context` is not valid as the
  // `ExtensionSystem` will not exist.
  if (!browser_context()) {
    Release();  // Matches the `AddRef()` in `Run()`.
    return;
  }

  // We cannot proceed if the extension is uninstalled or unloaded in the middle
  // of validating its script files.
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  if (!extension() ||
      !registry->enabled_extensions().Contains(extension_id())) {
    // Note: a Respond() is not needed if the system is shutting down or if the
    // extension is no longer enabled.
    Release();  // Matches the `AddRef()` in `Run()`.
    return;
  }

  auto error = std::move(result.second);
  auto scripts = std::move(result.first);
  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());

  bool allowed_in_incognito = scripting::ScriptsShouldBeAllowedInIncognito(
      extension()->id(), browser_context());

  std::set<std::string> script_ids;
  for (const auto& script : scripts) {
    script_ids.insert(script->id());
    script->set_incognito_enabled(allowed_in_incognito);
  }

  if (error.has_value()) {
    loader->RemovePendingDynamicScriptIDs(script_ids);
    Respond(Error(std::move(*error)));
    Release();  // Matches the `AddRef()` in `Run()`.
    return;
  }

  // User scripts are always persisted across sessions.
  std::set<std::string> persistent_script_ids = script_ids;
  loader->UpdateDynamicScripts(
      std::move(scripts), std::move(script_ids),
      std::move(persistent_script_ids),
      base::BindOnce(&UserScriptsUpdateFunction::OnUserScriptsUpdated, this));
}

void UserScriptsUpdateFunction::OnUserScriptsUpdated(
    const std::optional<std::string>& error) {
  if (error.has_value()) {
    Respond(Error(std::move(*error)));
  } else {
    Respond(NoArguments());
  }
  Release();  // Matches the `AddRef()` in `Run()`.
}

ExtensionFunction::ResponseAction UserScriptsConfigureWorldFunction::Run() {
  std::optional<api::user_scripts::ConfigureWorld::Params> params(
      api::user_scripts::ConfigureWorld::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(extension());

  std::optional<std::string> csp = std::move(params->properties.csp);
  bool enable_messaging = params->properties.messaging.value_or(false);

  std::optional<std::string> world_id;
  if (base::FeatureList::IsEnabled(
          extensions_features::kApiUserScriptsMultipleWorlds)) {
    world_id = std::move(params->properties.world_id);
  }

  std::string error;
  if (!IsValidWorldId(world_id, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  UserScriptWorldConfigurationManager* config_manager =
      UserScriptWorldConfigurationManager::Get(browser_context());
  static constexpr size_t kMaxNumberOfRegisteredWorlds = 100;
  if (config_manager->GetAllUserScriptWorlds(extension()->id()).size() >=
      kMaxNumberOfRegisteredWorlds) {
    return RespondNow(
        Error(base::StringPrintf("You may only configure up to %" PRIuS
                                 " individual user script worlds.",
                                 kMaxNumberOfRegisteredWorlds)));
  }

  config_manager->SetUserScriptWorldInfo(*extension(), world_id, csp,
                                         enable_messaging);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
UserScriptsGetWorldConfigurationsFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(extension());

  std::vector<mojom::UserScriptWorldInfoPtr> world_configurations =
      UserScriptWorldConfigurationManager::Get(browser_context())
          ->GetAllUserScriptWorlds(extension()->id());

  std::vector<api::user_scripts::WorldProperties> result;
  result.reserve(world_configurations.size());
  for (const auto& world : world_configurations) {
    api::user_scripts::WorldProperties converted;
    converted.messaging = world->enable_messaging;
    converted.csp = world->csp;
    converted.world_id = world->world_id;
    result.push_back(std::move(converted));
  }

  return RespondNow(ArgumentList(
      api::user_scripts::GetWorldConfigurations::Results::Create(result)));
}

ExtensionFunction::ResponseAction
UserScriptsResetWorldConfigurationFunction::Run() {
  std::optional<api::user_scripts::ResetWorldConfiguration::Params> params(
      api::user_scripts::ResetWorldConfiguration::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(extension());

  // In theory, it'd be safe to just pass in `world_id` without validating it
  // because we should never have an invalid world ID in the preferences. But
  // that's a fragile guarantee and may change if e.g. we start using reserved
  // world IDs. Validate to be on the safe side.
  std::string error;
  if (!IsValidWorldId(params->world_id, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  UserScriptWorldConfigurationManager::Get(browser_context())
      ->ClearUserScriptWorldInfo(*extension(), params->world_id);

  return RespondNow(NoArguments());
}

}  // namespace extensions
