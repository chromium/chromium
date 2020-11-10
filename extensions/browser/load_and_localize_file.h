// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LOAD_AND_LOCALIZE_FILE_H_
#define EXTENSIONS_BROWSER_LOAD_AND_LOCALIZE_FILE_H_

#include <memory>
#include <string>

#include "base/callback.h"

namespace extensions {
class Extension;
class ExtensionResource;

// Invoked with the result of the file read and localization. The bool
// indicates success and failure. On success, |data| contains the
// localized content of the file.
// TODO(devlin): Update this to just pass |data| and have null indicate
// failure once FileReader's callback signature is updated.
using LoadAndLocalizeResourceCallback =
    base::OnceCallback<void(bool success, std::unique_ptr<std::string> data)>;

// Loads |resource| from |extension|, optionally localizing the content, and
// invokes |callback| with the result. Handles both component and non-component
// extension resources. |resource| must be valid. Note: |callback| is always
// invoked asynchronously.
void LoadAndLocalizeResource(const Extension& extension,
                             const ExtensionResource& resource,
                             bool localize_file,
                             LoadAndLocalizeResourceCallback callback);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LOAD_AND_LOCALIZE_FILE_H_
