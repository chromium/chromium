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

// Attempts to deserialize `serialized_script` into a new `UserScript`. This can
// fail if `serialized_script` has invalid values for parsed types (e.g.,
// match patterns). `allowed_in_incognito` indicates if the corresponding
// extension (and thus, user script) is allowed in incognito mode.
// If `error_out` is provided, it will be populated on failure.
// If `wants_file_access_out` is provided, it will be populated with whether the
// extension wants file access according to the patterns in the serialized
// script.
// If `index_for_error` is populated, it will be used in the error message.
// If `custom_schemes` is provided, they will be used instead of the default
// schemes for URLPattern parsing.
// If `can_execute_script_everywhere` is true, it indicates the extension
// doesn't need additional file access permissions.
// If `all_urls_includes_chrome_urls` is true, <all_urls> patterns will also
// include chrome:-scheme URLs.
// TODO(devlin): It'd be nice to use absl::optional here, but UserScripts are
// currently passed by pointer a lot.
// TODO(devlin): Pull most/all these optional parameters out into a struct to
// pass in.
std::unique_ptr<UserScript> ParseSerializedUserScript(
    const api::scripts_internal::SerializedUserScript& serialized_script,
    const Extension& extension,
    bool allowed_in_incognito,
    std::u16string* error_out = nullptr,
    bool* wants_file_access_out = nullptr,
    absl::optional<int> index_for_error = absl::nullopt,
    absl::optional<int> custom_schemes = absl::nullopt,
    absl::optional<bool> can_execute_script_everywhere = absl::nullopt,
    bool all_urls_includes_chrome_urls = false);

}  // namespace script_serialization
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_SCRIPTS_INTERNAL_SCRIPT_SERIALIZATION_H_
