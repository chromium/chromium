// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/scripting_utils.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/scripting_constants.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/user_script.h"
#include "extensions/common/utils/content_script_utils.h"

namespace extensions::scripting {

namespace {

constexpr char kEmptyScriptIdError[] = "Script's ID must not be empty";
constexpr char kFilesExceededSizeLimitError[] =
    "Scripts could not be loaded because '*' exceeds the maximum script size "
    "or the extension's maximum total script size.";
constexpr char kNonExistentScriptIdError[] = "Nonexistent script ID '*'";
// Key corresponding to the set of URL patterns from the extension's persistent
// dynamic content scripts.
constexpr const char kPrefPersistentScriptURLPatterns[] =
    "persistent_script_url_patterns";
constexpr char kReservedScriptIdPrefixError[] =
    "Script's ID '*' must not start with '*'";

}  // namespace

std::string AddPrefixToDynamicScriptId(const std::string& script_id,
                                       UserScript::Source source) {
  std::string prefix;
  switch (source) {
    case UserScript::Source::kDynamicContentScript:
      prefix = UserScript::kDynamicContentScriptPrefix;
      break;
    case UserScript::Source::kDynamicUserScript:
      prefix = UserScript::kDynamicUserScriptPrefix;
      break;
    case UserScript::Source::kStaticContentScript:
    case UserScript::Source::kWebUIScript:
      NOTREACHED();
  }

  return prefix + script_id;
}

bool IsScriptIdValid(const std::string& script_id, std::string* error) {
  if (script_id.empty()) {
    *error = kEmptyScriptIdError;
    return false;
  }

  if (script_id[0] == UserScript::kReservedScriptIDPrefix) {
    *error = ErrorUtils::FormatErrorMessage(
        kReservedScriptIdPrefixError, script_id,
        std::string(1, UserScript::kReservedScriptIDPrefix));
    return false;
  }

  return true;
}

bool ScriptsShouldBeAllowedInIncognito(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context) {
  // Note: We explicitly use `util::IsIncognitoEnabled()` (and not
  // `ExtensionFunction::include_incognito_information()`) since the latter
  // excludes the on-the-record context of a split-mode extension. Since user
  // scripts are shared across profiles, we should use the overall setting for
  // the extension.
  return util::IsIncognitoEnabled(extension_id, browser_context);
}

bool RemoveScripts(
    const std::optional<std::vector<std::string>>& ids,
    UserScript::Source source,
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    ExtensionUserScriptLoader::DynamicScriptsModifiedCallback remove_callback,
    std::string* error) {
  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context)
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension_id);

  // Remove all scripts if ids are not provided. This doesn't include when ids
  // has a value, but it's empty.
  if (!ids.has_value()) {
    loader->ClearDynamicScripts(source, std::move(remove_callback));
    return true;
  }

  std::set<std::string> ids_to_remove;
  std::set<std::string> existing_script_ids =
      loader->GetDynamicScriptIDs(source);

  for (const auto& id : *ids) {
    if (!scripting::IsScriptIdValid(id, error)) {
      return false;
    }

    // Add the dynamic script prefix to `provided_id` before checking against
    // `existing_script_ids`.
    std::string id_with_prefix =
        scripting::AddPrefixToDynamicScriptId(id, source);
    if (!base::Contains(existing_script_ids, id_with_prefix)) {
      *error =
          ErrorUtils::FormatErrorMessage(kNonExistentScriptIdError, id.c_str());
      return false;
    }

    ids_to_remove.insert(id_with_prefix);
  }

  loader->RemoveDynamicScripts(std::move(ids_to_remove),
                               std::move(remove_callback));
  return true;
}

URLPatternSet GetPersistentScriptURLPatterns(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id) {
  URLPatternSet patterns;
  ExtensionPrefs::Get(browser_context)
      ->ReadPrefAsURLPatternSet(extension_id, kPrefPersistentScriptURLPatterns,
                                &patterns,
                                UserScript::ValidUserScriptSchemes());

  return patterns;
}

void SetPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                    const ExtensionId& extension_id,
                                    const URLPatternSet& patterns) {
  ExtensionPrefs::Get(browser_context)
      ->SetExtensionPrefURLPatternSet(
          extension_id, kPrefPersistentScriptURLPatterns, patterns);
}

void ClearPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                      const ExtensionId& extension_id) {
  ExtensionPrefs::Get(browser_context)
      ->UpdateExtensionPref(extension_id, kPrefPersistentScriptURLPatterns,
                            std::nullopt);
}

ValidateScriptsResult ValidateParsedScriptsOnFileThread(
    ExtensionResource::SymlinkPolicy symlink_policy,
    UserScriptList scripts) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  // Validate that claimed script resources actually exist, and are UTF-8
  // encoded.
  std::string error;
  std::vector<InstallWarning> warnings;
  bool are_script_files_valid = script_parsing::ValidateFileSources(
      scripts, symlink_policy, &error, &warnings);

  // Script files over the per script/extension size limit are recorded as
  // warnings. However, for this case we should treat "install warnings" as
  // errors by turning this call into a no-op and returning an error.
  if (!warnings.empty() && error.empty()) {
    error = ErrorUtils::FormatErrorMessage(kFilesExceededSizeLimitError,
                                           warnings[0].specific);
    are_script_files_valid = false;
  }

  return std::make_pair(std::move(scripts), are_script_files_valid
                                                ? std::nullopt
                                                : std::make_optional(error));
}

}  // namespace extensions::scripting
