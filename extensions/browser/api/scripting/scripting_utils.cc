// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/scripting/scripting_utils.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/scripting/scripting_constants.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/user_script.h"
#include "extensions/common/utils/content_script_utils.h"

namespace extensions::scripting {

namespace {

constexpr char kDuplicateScriptId[] = "Duplicate script ID '*'";
constexpr char kEmptyScriptIdError[] = "Script's ID must not be empty";
constexpr char kFilesExceededSizeLimitError[] =
    "Scripts could not be loaded because '*' exceeds the maximum script size "
    "or the extension's maximum total script size.";
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
      NOTREACHED_NORETURN();
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

std::string CreateDynamicScriptId(
    const std::string& script_id,
    UserScript::Source source,
    const std::set<std::string>& existing_script_ids,
    const std::set<std::string>& new_script_ids,
    std::string* error) {
  if (!IsScriptIdValid(script_id, error)) {
    return std::string();
  }

  std::string new_script_id =
      scripting::AddPrefixToDynamicScriptId(script_id, source);
  if (base::Contains(existing_script_ids, new_script_id) ||
      base::Contains(new_script_ids, new_script_id)) {
    *error =
        ErrorUtils::FormatErrorMessage(kDuplicateScriptId, script_id.c_str());
    return std::string();
  }

  return new_script_id;
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
                            absl::nullopt);
}

ValidateScriptsResult ValidateParsedScriptsOnFileThread(
    ExtensionResource::SymlinkPolicy symlink_policy,
    std::unique_ptr<UserScriptList> scripts) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  // Validate that claimed script resources actually exist, and are UTF-8
  // encoded.
  std::string error;
  std::vector<InstallWarning> warnings;
  bool are_script_files_valid = script_parsing::ValidateFileSources(
      *scripts, symlink_policy, &error, &warnings);

  // Script files over the per script/extension size limit are recorded as
  // warnings. However, for this case we should treat "install warnings" as
  // errors by turning this call into a no-op and returning an error.
  if (!warnings.empty() && error.empty()) {
    error = ErrorUtils::FormatErrorMessage(kFilesExceededSizeLimitError,
                                           warnings[0].specific);
    are_script_files_valid = false;
  }

  return std::make_pair(std::move(scripts), are_script_files_valid
                                                ? absl::nullopt
                                                : absl::make_optional(error));
}

}  // namespace extensions::scripting
