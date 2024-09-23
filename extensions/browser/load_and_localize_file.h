// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LOAD_AND_LOCALIZE_FILE_H_
#define EXTENSIONS_BROWSER_LOAD_AND_LOCALIZE_FILE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace extensions {
class Extension;
class ExtensionResource;

// Invoked with the result of the file read and localization.
// `data` is a vector that contains the result of the localized content of the
// files. `error` indicates the error, if any.
using LoadAndLocalizeResourcesCallback =
    base::OnceCallback<void(std::vector<std::unique_ptr<std::string>> data,
                            std::optional<std::string> error)>;

// Loads |resources| from |extension|, optionally localizing the content, and
// invokes |callback| with the result. Handles both component and non-component
// extension resources. |resources| must be valid. Note: |callback| is always
// invoked asynchronously.
void LoadAndLocalizeResources(const Extension& extension,
                              std::vector<ExtensionResource> resources,
                              bool localize_files,
                              size_t max_script_length,
                              LoadAndLocalizeResourcesCallback callback);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LOAD_AND_LOCALIZE_FILE_H_
