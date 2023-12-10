// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_UTILS_CONTENT_SCRIPT_UTILS_H_
#define EXTENSIONS_COMMON_UTILS_CONTENT_SCRIPT_UTILS_H_

#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/api/scripts_internal.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/user_script.h"

// Contains helper methods for parsing content script fields.

namespace extensions::script_parsing {

// Returns the maximum length allowed in an individual script file. Scripts
// above this length will not be loaded.
size_t GetMaxScriptLength();

// Returns the maximum allowed total length for all scripts loaded for a single
// extension. Any scripts past this limit will not be loaded.
size_t GetMaxScriptsLengthPerExtension();

using ScopedMaxScriptLengthOverride = base::AutoReset<size_t>;

// Temporarily sets the max per-file limit to `max`. The value gets reset once
// the AutoReset falls out of scope and is destroyed.
ScopedMaxScriptLengthOverride CreateScopedMaxScriptLengthForTesting(size_t max);

// Temporarily sets the per-extension limit to `max`. The value gets reset once
// the AutoReset falls out of scope and is destroyed.
ScopedMaxScriptLengthOverride
CreateScopedMaxScriptsLengthPerExtensionForTesting(size_t max);

// Parses and validates `matches` and `exclude_matches`, and updates these
// fields for `result`. If `wants_file_access` is not null, then it will be set
// to signal to the caller that the extension is requesting file access based
// the match patterns specified. `definition_index` must be only provided for
// static scripts.
bool ParseMatchPatterns(const std::vector<std::string>& matches,
                        const std::vector<std::string>* exclude_matches,
                        int creation_flags,
                        bool can_execute_script_everywhere,
                        bool all_urls_includes_chrome_urls,
                        std::optional<int> definition_index,
                        UserScript* result,
                        std::u16string* error,
                        bool* wants_file_access);

// Parses the `js` and `css` fields, and updates `result` with the specified
// file paths. Returns false and populates `error` if both `js` and `css` are
// empty. `definition_index` must be only provided for static scripts.
bool ParseFileSources(
    const Extension* extension,
    const std::vector<api::scripts_internal::ScriptSource>* js,
    const std::vector<api::scripts_internal::ScriptSource>* css,
    std::optional<int> definition_index,
    UserScript* result,
    std::u16string* error);

// Parses `include_globs` and `exclude_globs` and updates these fields for
// `result`. Done for Greasemonkey compatibility.
void ParseGlobs(const std::vector<std::string>* include_globs,
                const std::vector<std::string>* exclude_globs,
                UserScript* result);

// Validates that the claimed file sources in `scripts` actually exist and are
// UTF-8 encoded. This function must be called on a sequence which allows file
// I/O.
bool ValidateFileSources(const UserScriptList& scripts,
                         ExtensionResource::SymlinkPolicy symlink_policy,
                         std::string* error,
                         std::vector<InstallWarning>* warnings);

// Validates that `match_origin_as_fallback` is legal in relation to the match
// patterns specified in `url_patterns`. I.e. patterns in `url_patterns` must
// specify a wildcard path or no path if `match_origin_as_fallback` is enabled.
bool ValidateMatchOriginAsFallback(
    MatchOriginAsFallbackBehavior match_origin_as_fallback,
    const URLPatternSet& url_patterns,
    std::u16string* error_out);

ExtensionResource::SymlinkPolicy GetSymlinkPolicy(const Extension* extension);

}  // namespace extensions::script_parsing

#endif  // EXTENSIONS_COMMON_UTILS_CONTENT_SCRIPT_UTILS_H_
