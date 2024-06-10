// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCRIPTING_UTILS_H_
#define EXTENSIONS_BROWSER_SCRIPTING_UTILS_H_

#include <string>

#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions::scripting {

// Appends the prefix corresponding to the dynamic script `source` to
// `script_id`.
std::string AddPrefixToDynamicScriptId(const std::string& script_id,
                                       UserScript::Source source);

// Returns whether the extension provided `script_id` (without an internal
// prefix) is valid. Populates `error` if invalid.
bool IsScriptIdValid(const std::string& script_id, std::string* error);

// Returns whether new scripts added for the extension with the given
// `extension_id` should be allowed in incognito contexts.
bool ScriptsShouldBeAllowedInIncognito(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context);

// Returns a set of unique dynamic script IDs (with an added prefix
// corresponding to `source`) for all given `scripts`. If the script is invalid
// or duplicated in `existing_script_ids` or the new ids, populates error and
// returns an empty set.
template <typename Script>
std::set<std::string> CreateDynamicScriptIds(
    std::vector<Script>& scripts,
    UserScript::Source source,
    const std::set<std::string>& existing_script_ids,
    std::string* error) {
  std::set<std::string> new_script_ids;

  for (auto& script : scripts) {
    if (!IsScriptIdValid(script.id, error)) {
      return std::set<std::string>();
    }

    std::string new_script_id =
        scripting::AddPrefixToDynamicScriptId(script.id, source);
    if (base::Contains(existing_script_ids, new_script_id) ||
        base::Contains(new_script_ids, new_script_id)) {
      *error = ErrorUtils::FormatErrorMessage("Duplicate script ID '*'",
                                              script.id.c_str());
      return std::set<std::string>();
    }

    script.id = new_script_id;
    new_script_ids.insert(script.id);
  }

  return new_script_ids;
}

// Returns a list of UserScript objects for each `scripts_to_update` by
// retrieving the metadata of all loaded scripts with `source` using
// `create_script_callback` and updating them with the given delta using
// `apply_update_callback`. If any of the `scripts_to_update` hasn't been
// previously loaded or parsing fails, populates error and returns nullptr.
template <typename Script>
UserScriptList UpdateScripts(
    std::vector<Script>& scripts_to_update,
    UserScript::Source source,
    ExtensionUserScriptLoader& loader,
    base::RepeatingCallback<Script(const UserScript& script)>
        create_script_metadata_callback,
    base::RepeatingCallback<std::unique_ptr<UserScript>(
        Script& new_script,
        Script& existent_script,
        std::u16string* parse_error)> apply_update_callback,
    std::string* error) {
  // Retrieve the metadata of all loaded scripts with `source`.
  std::map<std::string, Script> loaded_scripts_metadata;
  const UserScriptList& dynamic_scripts = loader.GetLoadedDynamicScripts();
  for (const std::unique_ptr<UserScript>& script : dynamic_scripts) {
    if (script->GetSource() != source) {
      continue;
    }

    Script script_metadata = create_script_metadata_callback.Run(*script);
    loaded_scripts_metadata.emplace(script->id(), std::move(script_metadata));
  }

  // Verify scripts to update have previously been loaded.
  for (const auto& script : scripts_to_update) {
    if (!loaded_scripts_metadata.contains(script.id)) {
      *error = ErrorUtils::FormatErrorMessage(
          "Script with ID '*' does not exist or is not fully registered",
          UserScript::TrimPrefixFromScriptID(script.id));
      return {};
    }
  }

  // Update the scripts.
  std::u16string parse_error;
  UserScriptList parsed_scripts;
  parsed_scripts.reserve(scripts_to_update.size());
  for (Script& new_script : scripts_to_update) {
    CHECK(base::Contains(loaded_scripts_metadata, new_script.id));
    Script& existent_script = loaded_scripts_metadata[new_script.id];

    // Note: `new_script` and `existent_script` may be unsafe to use after this.
    std::unique_ptr<UserScript> script =
        apply_update_callback.Run(new_script, existent_script, &parse_error);
    if (!script) {
      CHECK(!parse_error.empty());
      *error = base::UTF16ToASCII(parse_error);
      return {};
    }

    parsed_scripts.push_back(std::move(script));
  }

  return parsed_scripts;
}

// Removes all scripts with `ids` of `extension_id`. If `ids` has no value,
// clears all scripts with `source` and `extension_id`. If any of the `ids`
// provided is invalid, populates `error` and returns false. Otherwise, returns
// true and removes the script from the UserScriptLoader invoking
// `remove_callback` on completion.
bool RemoveScripts(
    const std::optional<std::vector<std::string>>& ids,
    UserScript::Source source,
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    ExtensionUserScriptLoader::DynamicScriptsModifiedCallback remove_callback,
    std::string* error);

// Returns the set of URL patterns from persistent dynamic content scripts.
// Patterns are stored in prefs so UserScriptListener can access them
// synchronously as the persistent scripts themselves are stored in a
// StateStore.
URLPatternSet GetPersistentScriptURLPatterns(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id);

// Updates the set of URL patterns from persistent dynamic content scripts. This
// preference gets cleared on extension update.
void SetPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                    const ExtensionId& extension_id,
                                    const URLPatternSet& patterns);

// Clears the set of URL patterns from persistent dynamic content scripts.
void ClearPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                      const ExtensionId& extension_id);

// Holds a list of user scripts as the first item, or an error string as the
// second item when the user scripts are invalid.
using ValidateScriptsResult =
    std::pair<UserScriptList, std::optional<std::string>>;

// Validates that `scripts` resources exist and are properly encoded.
ValidateScriptsResult ValidateParsedScriptsOnFileThread(
    ExtensionResource::SymlinkPolicy symlink_policy,
    UserScriptList scripts);

}  // namespace extensions::scripting

#endif  // EXTENSIONS_BROWSER_SCRIPTING_UTILS_H_
