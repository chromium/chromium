// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FUNCTION_CRASH_KEYS_H_
#define EXTENSIONS_BROWSER_EXTENSION_FUNCTION_CRASH_KEYS_H_

#include "extensions/common/extension_id.h"

namespace extensions::extension_function_crash_keys {

// Records that an extension with `extension_id` is about to make an extension
// API call and run an ExtensionFunction. This updates a list of crash keys with
// the IDs of extensions with in-flight API calls.
void StartExtensionFunctionCall(const ExtensionId& extension_id);

// Records that an extension with `extension_id` finished making an extension
// API call. This updates a list of crash keys with the IDs of extensions with
// in-flight API calls. A call to this function must be proceeded by a call to
// StartExtensionFunctionCall() otherwise this function will CHECK.
void EndExtensionFunctionCall(const ExtensionId& extension_id);

}  // namespace extensions::extension_function_crash_keys

#endif  // EXTENSIONS_BROWSER_EXTENSION_FUNCTION_CRASH_KEYS_H_
