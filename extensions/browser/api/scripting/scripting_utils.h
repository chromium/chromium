// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_UTILS_H_
#define EXTENSIONS_BROWSER_API_SCRIPTING_SCRIPTING_UTILS_H_

#include "extensions/browser/extension_user_script_loader.h"
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

// Returns a dynamic script ID (with a prefix corresponding to `source`)
// when the script is valid and is not duplicated in `existing_script_ids` or
// `new_script_ids`. Otherwise populates error and returns an empty string.
std::string CreateDynamicScriptId(
    const std::string& script_id,
    UserScript::Source source,
    const std::set<std::string>& existing_script_ids,
    const std::set<std::string>& new_script_ids,
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
