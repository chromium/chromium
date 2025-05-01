// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_USER_SCRIPTS_ALLOWED_STATE_H_
#define EXTENSIONS_COMMON_USER_SCRIPTS_ALLOWED_STATE_H_

#include <optional>

#include "extensions/common/extension_id.h"

namespace extensions {

// Returns the current user script allowed state for the given extension ID for
// the specific context. If the state has never been set then the optional will
// not have a value.
std::optional<bool> GetCurrentUserScriptAllowedState(
    int context_id,
    const ExtensionId& extension_id);

// Sets the user script allowed state for the given extension ID in the specific
// context.
void SetCurrentUserScriptAllowedState(int context_id,
                                      const ExtensionId& extension_id,
                                      bool enabled);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_USER_SCRIPTS_ALLOWED_STATE_H_
