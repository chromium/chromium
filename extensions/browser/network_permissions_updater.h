// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_NETWORK_PERMISSIONS_UPDATER_H_
#define EXTENSIONS_BROWSER_NETWORK_PERMISSIONS_UPDATER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;

// A helper class to update the network service's records of extension
// permissions.
// This class effectively manages its own lifetime (via a unique_ptr).
// TODO(devlin): With a bit more finagling, we can bring most of the CORS-
// updating logic from extension_util into this file (the main piece missing
// is to add in a toggle to control whether related contexts are included). We
// should do that to centralize this logic and reduce the number of ambiguous
// "util"-style functions we have.
class NetworkPermissionsUpdater {
 public:
  using PassKey = base::PassKey<NetworkPermissionsUpdater>;

  // The contexts to include for when updating the extension.
  enum class ContextSet {
    // Only the current context will be updated. Use this when the permission
    // is related to a specific context (like a specific tab).
    kCurrentContextOnly,
    // All related contexts the extension is allowed to run in will be updated.
    // Use this when the permission is related to both contexts (like a
    // permission grant on the extension).
    kAllRelatedContexts,
  };

  // Pseudo-private ctor. This is public so that it can be used with
  // std::make_unique<>, but guarded via the PassKey. Consumers should only use
  // the static methods below.
  NetworkPermissionsUpdater(PassKey pass_key,
                            content::BrowserContext& browser_context,
                            base::OnceClosure completion_callback);
  ~NetworkPermissionsUpdater();

  // Updates a single `extension`'s permissions in the network layer. Invokes
  // `completion_callback` when the operation is complete.
  static void UpdateExtension(content::BrowserContext& browser_context,
                              const Extension& extension,
                              ContextSet context_set,
                              base::OnceClosure completion_callback);

  // Updates the permissions of all extensions related to the (original)
  // `browser_context`. Invokes `completion_callback` when the operation is
  // complete.
  // Updating all extensions always uses `ContextSet::kAllRelatedContexts` as
  // there (currently) are no situations in which all extensions should be
  // updated for a context-specific reason.
  static void UpdateAllExtensions(content::BrowserContext& browser_context,
                                  base::OnceClosure completion_callback);

  // Resets the origin allowlist and blocklist for `extension` to empty lists
  // for `browser_context`. This only affects the specified `browser_context`;
  // it does not affect any related (incognito) contexts.
  // TODO(devlin/lukasza): Should it?
  static void ResetOriginAccessForExtension(
      content::BrowserContext& browser_context,
      const Extension& extension);

 private:
  // Updates a single extension in the network layer, invoking
  // `completion_callback` when the operation is complete.
  void UpdateExtension(const Extension& extension,
                       ContextSet context_set,
                       base::OnceClosure completion_callback);

  // Invoked when all updates are complete in order to dispatch
  // `completion_callback_`.
  static void OnOriginAccessUpdated(
      std::unique_ptr<NetworkPermissionsUpdater> updater);

  // The associated BrowserContext.
  raw_ptr<content::BrowserContext, DanglingUntriaged> const browser_context_;

  // A callback to invoke upon completion.
  base::OnceClosure completion_callback_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_NETWORK_PERMISSIONS_UPDATER_H_
