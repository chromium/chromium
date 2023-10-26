// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SCRIPTS_INTERNAL_SCRIPT_SERIALIZATION_H_
#define EXTENSIONS_BROWSER_API_SCRIPTS_INTERNAL_SCRIPT_SERIALIZATION_H_

#include <memory>
#include <string>

#include "extensions/common/api/scripts_internal.h"
#include "extensions/common/user_script.h"

namespace extensions {
class Extension;

namespace script_serialization {

// Serialized the given `user_script`. This is guaranteed to succeed (assuming
// `user_script` is valid).
api::scripts_internal::SerializedUserScript SerializeUserScript(
    const UserScript& user_script);

// Attempts to deserialize `serialized_script` into a new `UserScript`. This can
// fail if `serialized_script` has invalid values for parsed types (e.g.,
// match patterns). `allowed_in_incognito` indicates if the corresponding
// extension (and thus, user script) is allowed in incognito mode.
// If `error_out` is provided, it will be populated on failure.
// TODO(devlin): It'd be nice to use absl::optional here, but UserScripts are
// currently passed by pointer a lot.
std::unique_ptr<UserScript> ParseSerializedUserScript(
    const api::scripts_internal::SerializedUserScript& serialized_script,
    const Extension& extension,
    bool allowed_in_incognito,
    std::u16string* error_out = nullptr);

}  // namespace script_serialization
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SCRIPTS_INTERNAL_SCRIPT_SERIALIZATION_H_
