// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_STORAGE_UTILS_H_
#define EXTENSIONS_BROWSER_API_STORAGE_STORAGE_UTILS_H_

#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/storage/session_storage_manager.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/extension_id.h"

namespace extensions::storage_utils {

// Returns the session storage access level for `extension_id`.
api::storage::AccessLevel GetSessionAccessLevel(
    const ExtensionId& extension_id,
    content::BrowserContext& browser_context);

// Sets the session storage access level for `extension_id` to `access_level`.
void SetSessionAccessLevel(const ExtensionId& extension_id,
                           content::BrowserContext& browser_context,
                           api::storage::AccessLevel access_level);

// Returns a nested dictionary Value converted from a ValueChange.
base::Value ValueChangeToValue(
    std::vector<SessionStorageManager::ValueChange> changes);

}  // namespace extensions::storage_utils

#endif  // EXTENSIONS_BROWSER_API_STORAGE_STORAGE_UTILS_H_
