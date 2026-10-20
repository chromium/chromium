// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_REGISTRY_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_REGISTRY_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/browser_context_lifetime_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class MimeTypesHandler;

namespace content {
class BrowserContext;
}

namespace extensions {

class BrowserContextLifetimeTracker;
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
// Manifest mappings (`handlers_by_type_`) reflect installed extensions
// and are profile-unaware. Per-(extension, mime_type) enable flags are
// profile-scoped, stored in `ExtensionPrefs`, and applied uniformly by
// both `GetHandlersForMimeType()` and `GetHandlersByMimeType()` — a
// disabled handler is never surfaced. A manifest entry supplies the enable
// flag when none is stored. Callers layer profile-specific
// eligibility filtering on top of what the registry returns.
//
// The registry also serves the off-the-record contexts of its profile, so every
// getter and setter takes the context it is called for. For a split mode
// extension, calls from an off-the-record context write a separate set of
// options instead of the prefs, and read that set before the prefs. This set
// lives only in memory, is shared by all off-the-record contexts, and is
// cleared when the primary off-the-record context is destroyed.
class MimeHandlerRegistry : public KeyedService,
                            public ExtensionRegistryObserver,
                            public BrowserContextLifetimeObserver {
 public:
  // Returns the registry for `context`, creating it if needed.
  static MimeHandlerRegistry* Get(content::BrowserContext* context);

  // Ensures the factory singleton is constructed (for dependency ordering).
  static void EnsureFactoryBuilt();

  explicit MimeHandlerRegistry(content::BrowserContext* context);
  MimeHandlerRegistry(const MimeHandlerRegistry&) = delete;
  MimeHandlerRegistry& operator=(const MimeHandlerRegistry&) = delete;
  ~MimeHandlerRegistry() override;

  // Returns the ordered list of extension IDs registered for `mime_type`,
  // sorted descending by precedence: `front()` is the highest-precedence
  // candidate.
  // Disabled handlers are left out. Callers must apply profile-specific
  // eligibility filtering to the returned ids.
  std::vector<ExtensionId> GetHandlersForMimeType(
      const std::string& mime_type,
      content::BrowserContext* context) const;

  using HandlersByMimeType =
      base::flat_map<std::string, std::vector<ExtensionId>>;

  // Returns a map from every MIME type with at least one enabled handler
  // to its ordered handler list. Each list follows the same precedence
  // order as `GetHandlersForMimeType`.
  HandlersByMimeType GetHandlersByMimeType(
      content::BrowserContext* context) const;

  // Returns true if `extension_id` has MIME handling enabled for
  // `mime_type`. Defaults to the value declared in the manifest when no
  // preference is stored. Requires `extension_id` to be an enabled extension
  // that handles `mime_type`.
  bool IsEnabledForMimeType(const ExtensionId& extension_id,
                            const std::string& mime_type,
                            content::BrowserContext* context) const;

  // Returns true if `extension_id` may handle embedded loads of
  // `mime_type`. Defaults to the manifest's can_embed when no preference is
  // stored. Requires `extension_id` to be an enabled extension that handles
  // `mime_type`. The result does not depend on IsEnabledForMimeType().
  bool CanEmbedForMimeType(const ExtensionId& extension_id,
                           const std::string& mime_type,
                           content::BrowserContext* context) const;

  // Persists the enabled state for (`extension_id`, `mime_type`).
  // Changing the state requires `extension_id` to be an enabled extension
  // that handles `mime_type`. If `context` is off-the-record and the extension
  // runs in split mode, the state is kept in memory and applies only to
  // off-the-record contexts.
  void SetEnabledForMimeType(const ExtensionId& extension_id,
                             const std::string& mime_type,
                             bool enabled,
                             content::BrowserContext* context);

  // Persists the can_embed state for (`extension_id`, `mime_type`). Does not
  // change the list from GetHandlersForMimeType(). Requires `extension_id` to
  // be an enabled extension that handles `mime_type`. If `context` is
  // off-the-record and the extension runs in split mode, the state is kept in
  // memory and applies only to off-the-record contexts.
  void SetCanEmbedForMimeType(const ExtensionId& extension_id,
                              const std::string& mime_type,
                              bool can_embed,
                              content::BrowserContext* context);

  // BrowserContextLifetimeObserver:
  void OnRelatedOffTheRecordBrowserContextDestroyed(
      content::BrowserContext& off_the_record_context) override;

 private:
  // ExtensionRegistryObserver:
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  // Registers `extension` as a MIME handler of every MIME type it declares.
  void RegisterExtension(const Extension* extension);

  // Removes all mappings for `extension_id`.
  void UnregisterExtension(const ExtensionId& extension_id);

  // Returns the MIME handler of `extension_id`, which must be an enabled
  // extension that handles `mime_type`.
  const MimeTypesHandler& GetHandlerOfMimeType(
      const ExtensionId& extension_id,
      const std::string& mime_type) const;

  // Returns the handlers in `handlers` that are enabled for `mime_type` in
  // `context`, in the same order.
  std::vector<ExtensionId> EnabledHandlers(
      const std::string& mime_type,
      const std::vector<ExtensionId>& handlers,
      content::BrowserContext* context) const;

  // Returns the value stored under `key` for (`extension_id`, `mime_type`),
  // or nullopt when the extension has not set it. If `context` is off the
  // record and the extension runs in split incognito mode, reads
  // `incognito_options_` first and the prefs when `incognito_options_` holds
  // no value.
  std::optional<bool> ReadStoredOption(const ExtensionId& extension_id,
                                       const std::string& mime_type,
                                       std::string_view key,
                                       content::BrowserContext* context) const;

  // Stores `value` under `key` for (`extension_id`, `mime_type`), keeping the
  // other options of that MIME type. If `context` is off the record and the
  // extension runs in split incognito mode, writes to `incognito_options_`
  // instead of the prefs.
  void StoreOption(const ExtensionId& extension_id,
                   const std::string& mime_type,
                   std::string_view key,
                   bool value,
                   content::BrowserContext* context);

  // Sorts `handlers` in descending precedence order. See
  // `RegisterExtension` for the precedence rules.
  void SortByPrecedence(std::vector<ExtensionId>& handlers) const;

  const raw_ref<content::BrowserContext> browser_context_;

  // Reports when the primary off-the-record context is destroyed. Null if the
  // embedder does not provide a tracker.
  raw_ptr<BrowserContextLifetimeTracker> browser_context_lifetime_tracker_;

  // MIME type -> ordered extension IDs. Sorted descending by precedence,
  // so `front()` is the active handler. Lists every declared handler of a
  // loaded extension, enabled or not. See `RegisterExtension` for the full
  // precedence rules.
  HandlersByMimeType handlers_by_type_;

  // Options that split mode extensions set from off-the-record contexts, keyed
  // by extension ID. Each value has the format of the `mime_handler_options`
  // pref.
  base::flat_map<ExtensionId, base::DictValue> incognito_options_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_REGISTRY_H_
