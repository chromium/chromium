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
  std::set<std::string> existing_script_ids = loader->GetDynamicScriptIDs();
  std::set<std::string> new_script_ids;
  std::string error;

  for (auto& script : scripts) {
    script.id = scripting::CreateDynamicScriptId(
        script.id, UserScript::Source::kDynamicUserScript, existing_script_ids,
        new_script_ids, &error);
    if (script.id.empty()) {
      DCHECK(!error.empty());
      return RespondNow(Error(std::move(error)));
    }

    new_script_ids.insert(script.id);
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

}  // namespace extensions
