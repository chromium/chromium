// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_UTILS_H_
#define EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_UTILS_H_

#include "base/containers/contains.h"
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

// Removes all scripts with `ids` of `extension_id`. If `ids` has no value,
// clears all scripts with `source` and `extension_id`. If any of the `ids`
// provided is invalid, populates `error` and returns false. Otherwise, returns
// true and removes the script from the UserScriptLoader invoking
// `remove_callback` on completion.
bool RemoveScripts(
    const absl::optional<std::vector<std::string>>& ids,
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
    std::pair<std::unique_ptr<UserScriptList>, absl::optional<std::string>>;

// Validates that `scripts` resources exist and are properly encoded.
ValidateScriptsResult ValidateParsedScriptsOnFileThread(
    ExtensionResource::SymlinkPolicy symlink_policy,
    std::unique_ptr<UserScriptList> scripts);

}  // namespace extensions::scripting

#endif  // EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_UTILS_H_
