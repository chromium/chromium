// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_SCRIPTS_INTERNAL_SCRIPT_SERIALIZATION_H_
#define EXTENSIONS_COMMON_API_SCRIPTS_INTERNAL_SCRIPT_SERIALIZATION_H_

#include <memory>
#include <string>

#include "extensions/common/api/scripts_internal.h"
#include "extensions/common/user_script.h"

namespace extensions {
class Extension;

namespace script_serialization {

// Converts a list of file names into a list of `ScriptSource`s.
std::vector<api::scripts_internal::ScriptSource> GetSourcesFromFileNames(
    std::vector<std::string> file_names);

// Serialized the given `user_script`. This is guaranteed to succeed (assuming
// `user_script` is valid).
api::scripts_internal::SerializedUserScript SerializeUserScript(
    const UserScript& user_script);

// Additional options for parsing user scripts.
struct SerializedUserScriptParseOptions {
  // If populated, used in the error message.
  std::optional<int> index_for_error;
  // If true, indicates the extension can execute scripts on every page without
  // additional permission (this should only be true for special extensions like
  // ChromeVox).
  bool can_execute_script_everywhere = false;
  // If true, `<all_urls>` match patterns will also match chrome:-scheme URLs.
  bool all_urls_includes_chrome_urls = false;
};

// Attempts to deserialize `serialized_script` into a new `UserScript`. This can
// fail if `serialized_script` has invalid values for parsed types (e.g.,
// match patterns). `allowed_in_incognito` indicates if the corresponding
// extension (and thus, user script) is allowed in incognito mode.
// If `error_out` is provided, it will be populated on failure.
// If `wants_file_access_out` is provided, it will be populated with whether the
// extension wants file access according to the patterns in the serialized
// script.
// TODO(devlin): It'd be nice to use std::optional here, but UserScripts are
// currently passed by pointer a lot.
std::unique_ptr<UserScript> ParseSerializedUserScript(
    const api::scripts_internal::SerializedUserScript& serialized_script,
    const Extension& extension,
    bool allowed_in_incognito,
    std::u16string* error_out = nullptr,
    bool* wants_file_access_out = nullptr,
    SerializedUserScriptParseOptions parse_options = {});

}  // namespace script_serialization
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_SCRIPTS_INTERNAL_SCRIPT_SERIALIZATION_H_
