// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/user_scripts/user_scripts_api.h"

#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "extensions/browser/api/scripting/scripting_constants.h"
#include "extensions/browser/api/scripting/scripting_utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/api/user_scripts.h"
#include "extensions/common/user_script.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/common/utils/extension_types_utils.h"

namespace extensions {

namespace {

constexpr char kEmptySourceError[] =
    "User script with ID '*' must specify at least one js source.";
constexpr char kInvalidSourceError[] =
    "User script with ID '*' must specify exactly one of 'code' or 'file' as a "
    "js source.";
constexpr char kMatchesMissingError[] =
    "User script with ID '*' must specify 'matches'.";

std::unique_ptr<UserScript> ParseUserScript(
    const Extension& extension,
    const api::user_scripts::RegisteredUserScript& user_script,
    int definition_index,
    std::u16string* error) {
  auto result = std::make_unique<UserScript>();
  result->set_id(user_script.id);
  result->set_host_id(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension.id()));

  if (user_script.run_at != api::extension_types::RunAt::kNone) {
    result->set_run_location(ConvertRunLocation(user_script.run_at));
  }

  if (user_script.all_frames) {
    result->set_match_all_frames(*user_script.all_frames);
  }

  if (!user_script.matches) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        kMatchesMissingError,
        UserScript::TrimPrefixFromScriptID(user_script.id));
    return nullptr;
  }

  // TODO(crbug.com/1385165): Update error messages to not be specific to
  // scripting API. Eg: kInvalidMatch should not be specific to
  // 'content_scripts[*].matches'.
  const int valid_schemes = UserScript::ValidUserScriptSchemes(
      scripting::kScriptsCanExecuteEverywhere);
  if (!script_parsing::ParseMatchPatterns(
          *user_script.matches,
          base::OptionalToPtr(user_script.exclude_matches), definition_index,
          extension.creation_flags(), scripting::kScriptsCanExecuteEverywhere,
          valid_schemes, scripting::kAllUrlsIncludesChromeUrls, result.get(),
          error,
          /*wants_file_access=*/nullptr)) {
    return nullptr;
  }

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

    if (source.code) {
      // TODO(crbug.com/1385165): Register user scripts when code is given.
    } else {
      DCHECK(source.file);
      GURL url = extension.GetResourceURL(*source.file);
      ExtensionResource resource = extension.GetResource(*source.file);
      result->js_scripts().push_back(std::make_unique<UserScript::File>(
          resource.extension_root(), resource.relative_path(), url));
    }
  }

  return result;
}

// Converts a UserScript object to a api::user_scripts::RegisteredUserScript
// object, used for getScripts.
api::user_scripts::RegisteredUserScript CreateRegisteredUserScriptInfo(
    const UserScript& script) {
  api::user_scripts::RegisteredUserScript script_info;
  CHECK_EQ(UserScript::Source::kDynamicUserScript, script.GetSource());

  script_info.id = script.id();
  script_info.all_frames = script.match_all_frames();
  script_info.run_at = ConvertRunLocationForAPI(script.run_location());

  script_info.matches.emplace();
  script_info.matches->reserve(script.url_patterns().size());
  for (const URLPattern& pattern : script.url_patterns()) {
    script_info.matches->push_back(pattern.GetAsString());
  }

  if (!script.exclude_url_patterns().is_empty()) {
    script_info.exclude_matches.emplace();
    script_info.exclude_matches->reserve(script.exclude_url_patterns().size());
    for (const URLPattern& pattern : script.exclude_url_patterns()) {
      script_info.exclude_matches->push_back(pattern.GetAsString());
    }
  }

  // File paths may be normalized in the returned object and can differ slightly
  // compared to what was originally passed into userScripts.register.
  if (!script.js_scripts().empty()) {
    script_info.js.reserve(script.js_scripts().size());
    for (const auto& file : script.js_scripts()) {
      api::user_scripts::ScriptSource source;
      source.file = file->relative_path().AsUTF8Unsafe();
      script_info.js.push_back(std::move(source));
    }
  }

  return script_info;
}

}  // namespace

ExtensionFunction::ResponseAction UserScriptsRegisterFunction::Run() {
  absl::optional<api::user_scripts::Register::Params> params(
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
  auto parsed_scripts = std::make_unique<UserScriptList>();
  parsed_scripts->reserve(scripts.size());
  std::u16string parse_error;

  for (size_t i = 0; i < scripts.size(); ++i) {
    std::unique_ptr<UserScript> user_script =
        ParseUserScript(*extension(), scripts[i], i, &parse_error);
    if (!user_script) {
      return RespondNow(Error(base::UTF16ToASCII(parse_error)));
    }

    parsed_scripts->push_back(std::move(user_script));
  }

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

  // Balanced in `OnUserScriptFilesValidated()` or
  // `OnUserScriptFilesValidated()`.
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

  auto error = std::move(result.second);
  auto scripts = std::move(result.first);

  std::set<std::string> script_ids;
  for (const auto& script : *scripts) {
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
    const absl::optional<std::string>& error) {
  if (error.has_value()) {
    Respond(Error(std::move(*error)));
  } else {
    Respond(NoArguments());
  }
  Release();  // Matches the `AddRef()` in `Run()`.
}

ExtensionFunction::ResponseAction UserScriptsGetScriptsFunction::Run() {
  absl::optional<api::user_scripts::GetScripts::Params> params =
      api::user_scripts::GetScripts::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  absl::optional<api::user_scripts::UserScriptFilter>& filter = params->filter;
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
  absl::optional<api::user_scripts::Unregister::Params> params(
      api::user_scripts::Unregister::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(extension());

  absl::optional<api::user_scripts::UserScriptFilter>& filter = params->filter;
  absl::optional<std::vector<std::string>> ids = absl::nullopt;
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
    const absl::optional<std::string>& error) {
  if (error.has_value()) {
    Respond(Error(std::move(*error)));
  } else {
    Respond(NoArguments());
  }
}

}  // namespace extensions
