// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_REGISTRY_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_REGISTRY_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// Tracks which installed extensions are registered to handle a given MIME
// type, and orders the candidates by precedence.
//
// Precedence rules, applied in order:
//   1. Public (non-allowlisted) handlers supersede allowlisted ones.
//   2. Among public handlers, the most recently installed handler wins
//      (ordering key is `GetFirstInstallTime` from `install_prefs_helper.h`).
//   3. Among allowlisted handlers, a higher index in
//      `kMIMETypeHandlersAllowlist` wins.
//
// "Allowlisted" means the extension's id is present in
// `kMIMETypeHandlersAllowlist` (see
// `extensions/common/manifest_handlers/mime_types_handler.cc`). Precedence
// depends on allowlist membership only, not on manifest format.
//
// This registry lives in `extensions/` and is intentionally unaware of
// profile state (incognito, prefs, policy, managed mode). Callers must
// apply profile-specific eligibility filtering to the returned candidates
// before using a handler.
class MimeHandlerRegistry : public KeyedService,
                            public ExtensionRegistryObserver {
 public:
  // Returns the registry for `context`, creating it if needed.
  static MimeHandlerRegistry* Get(content::BrowserContext* context);

  // Ensures the factory singleton is constructed (for dependency ordering).
  static void EnsureFactoryBuilt();

  explicit MimeHandlerRegistry(content::BrowserContext* context);
  MimeHandlerRegistry(const MimeHandlerRegistry&) = delete;
  MimeHandlerRegistry& operator=(const MimeHandlerRegistry&) = delete;
  ~MimeHandlerRegistry() override;

  // Returns the highest-precedence extension ID that handles `mime_type`,
  // or an empty string if no registered handler exists. Thin wrapper over
  // `GetHandlersForMimeType` that returns the first element.
  ExtensionId GetHandlerForMimeType(const std::string& mime_type) const;

  // Returns the ordered list of extension IDs registered for `mime_type`,
  // sorted descending by precedence: `front()` is the active handler.
  // The returned span references storage owned by this registry and is
  // invalidated by any subsequent extension load or unload. Callers must
  // apply profile-specific eligibility filtering before using a returned
  // id.
  base::span<const ExtensionId> GetHandlersForMimeType(
      const std::string& mime_type) const;

  using HandlersByMimeType =
      base::flat_map<std::string, std::vector<ExtensionId>>;

  // Returns a map from every registered MIME type to its ordered handler
  // list. Each list follows the same precedence order as
  // `GetHandlersForMimeType`. Intended for callers that need to enumerate
  // all registered MIME types. The reference is invalidated by any
  // subsequent extension load or unload.
  const HandlersByMimeType& GetHandlersByMimeType() const;

 private:
  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Registers `extension` as a MIME handler.
  void RegisterExtension(const Extension* extension);

  // Removes all mappings for `extension_id`.
  void UnregisterExtension(const ExtensionId& extension_id);

  const raw_ref<content::BrowserContext> browser_context_;

  // MIME type -> ordered extension IDs. Sorted descending by precedence,
  // so `front()` is the active handler. See `RegisterExtension` for the
  // full precedence rules.
  HandlersByMimeType handlers_by_type_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_REGISTRY_H_
