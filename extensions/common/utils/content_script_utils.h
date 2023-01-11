// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_UTILS_CONTENT_SCRIPT_UTILS_H_
#define EXTENSIONS_COMMON_UTILS_CONTENT_SCRIPT_UTILS_H_

#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/user_script.h"

// Contains helper methods for parsing content script fields.

namespace extensions {
namespace script_parsing {

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

// Converts api::content_scripts::RunAt to mojom::RunLocation.
mojom::RunLocation ConvertManifestRunLocation(
    api::content_scripts::RunAt run_at);

// Converts mojom::RunLocation to api::content_scripts::RunAt.
api::content_scripts::RunAt ConvertRunLocationToManifestType(
    mojom::RunLocation run_at);

// Parses and validates `matches` and `exclude_matches`, and updates these
// fields for `result`. If `wants_file_access` is not null, then it will be set
// to signal to the caller that the extension is requesting file access based
// the match patterns specified.
bool ParseMatchPatterns(const std::vector<std::string>& matches,
                        const std::vector<std::string>* exclude_matches,
                        int definition_index,
                        int creation_flags,
                        bool can_execute_script_everywhere,
                        int valid_schemes,
                        bool all_urls_includes_chrome_urls,
                        UserScript* result,
                        std::u16string* error,
                        bool* wants_file_access);

// Parses the `js` and `css` fields, and updates `result` with the specified
// file paths. Returns false and populates `error` if both `js` and `css` are
// empty.
bool ParseFileSources(const Extension* extension,
                      const std::vector<std::string>* js,
                      const std::vector<std::string>* css,
                      int definition_index,
                      UserScript* result,
                      std::u16string* error);

// Validates that the claimed file sources in `scripts` actually exist and are
// UTF-8 encoded. This function must be called on a sequence which allows file
// I/O.
bool ValidateFileSources(const UserScriptList& scripts,
                         ExtensionResource::SymlinkPolicy symlink_policy,
                         std::string* error,
                         std::vector<InstallWarning>* warnings);

ExtensionResource::SymlinkPolicy GetSymlinkPolicy(const Extension* extension);

}  // namespace script_parsing
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_UTILS_CONTENT_SCRIPT_UTILS_H_
