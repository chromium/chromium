// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ERROR_TEST_UTIL_H_
#define EXTENSIONS_BROWSER_EXTENSION_ERROR_TEST_UTIL_H_

#include <memory>
#include <string>

#include "extensions/common/extension_id.h"

namespace extensions {

class ExtensionError;

namespace error_test_util {

// Create a new RuntimeError.
std::unique_ptr<ExtensionError> CreateNewRuntimeError(
    const ExtensionId& extension_id,
    const std::string& message,
    bool from_incognito);

// Create a new RuntimeError; incognito defaults to "false".
std::unique_ptr<ExtensionError> CreateNewRuntimeError(
    const ExtensionId& extension_id,
    const std::string& message);

// Create a new ManifestError.
std::unique_ptr<ExtensionError> CreateNewManifestError(
    const ExtensionId& extension_id,
    const std::string& message);

}  // namespace error_test_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ERROR_TEST_UTIL_H_
