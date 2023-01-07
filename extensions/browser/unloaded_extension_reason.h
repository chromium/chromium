// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UNLOADED_EXTENSION_REASON_H_
#define EXTENSIONS_BROWSER_UNLOADED_EXTENSION_REASON_H_

namespace extensions {

// Reasons an extension may have been unloaded.
enum class UnloadedExtensionReason {
  UNDEFINED,              // Undefined state used to initialize variables.
  DISABLE,                // Extension is being disabled.
  UPDATE,                 // Extension is being updated to a newer version.
  UNINSTALL,              // Extension is being uninstalled.
  TERMINATE,              // Extension has terminated.
  BLOCKLIST,              // Extension has been blocklisted.
  PROFILE_SHUTDOWN,       // Profile is being shut down.
  LOCK_ALL,               // All extensions for the profile are blocked.
  MIGRATED_TO_COMPONENT,  // Extension is being migrated to a component
                          // action.
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UNLOADED_EXTENSION_REASON_H_
