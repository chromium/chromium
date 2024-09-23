// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRY_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/version.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS) && \
    !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
#error "Extensions must be enabled"
#endif

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ExtensionRegistryObserver;
enum class UnloadedExtensionReason;

// ExtensionRegistry holds sets of the installed extensions for a given
// BrowserContext. An incognito browser context and its original browser context
// share a single registry.
class ExtensionRegistry : public KeyedService {
 public:
  // Flags to pass to GetExtensionById() to select which sets to look in.
  enum IncludeFlag {
    NONE = 0,
    ENABLED = 1 << 0,
    DISABLED = 1 << 1,
    TERMINATED = 1 << 2,
    BLOCKLISTED = 1 << 3,
    BLOCKED = 1 << 4,
    EVERYTHING = (1 << 5) - 1,
  };

  explicit ExtensionRegistry(content::BrowserContext* browser_context);

  ExtensionRegistry(const ExtensionRegistry&) = delete;
  ExtensionRegistry& operator=(const ExtensionRegistry&) = delete;

  ~ExtensionRegistry() override;

  // Returns the instance for the given |browser_context|.
  static ExtensionRegistry* Get(content::BrowserContext* browser_context);

  content::BrowserContext* browser_context() const { return browser_context_; }

  // NOTE: These sets are *eventually* mutually exclusive, but an extension can
  // appear in two sets for short periods of time.
  const ExtensionSet& enabled_extensions() const {
    return enabled_extensions_;
  }
  const ExtensionSet& disabled_extensions() const {
    return disabled_extensions_;
  }
  const ExtensionSet& terminated_extensions() const {
    return terminated_extensions_;
  }
  const ExtensionSet& blocklisted_extensions() const {
    return blocklisted_extensions_;
  }
  const ExtensionSet& blocked_extensions() const { return blocked_extensions_; }
  const ExtensionSet& ready_extensions() const { return ready_extensions_; }

  // Returns the set of all installed extensions, regardless of state (enabled,
  // disabled, etc). Equivalent to GenerateInstalledExtensionSet(EVERYTHING).
  ExtensionSet GenerateInstalledExtensionsSet() const;

  // Returns a set of all extensions in the subsets specified by |include_mask|.
  //  * enabled_extensions()     --> ExtensionRegistry::ENABLED
  //  * disabled_extensions()    --> ExtensionRegistry::DISABLED
  //  * terminated_extensions()  --> ExtensionRegistry::TERMINATED
  //  * blocklisted_extensions() --> ExtensionRegistry::BLOCKLISTED
  //  * blocked_extensions()     --> ExtensionRegistry::BLOCKED
  ExtensionSet GenerateInstalledExtensionsSet(int include_mask) const;

  // Returns the current version of the extension with the given |id|, if
  // one exists.
  // Note: If we are currently updating the extension, this returns the
  // version stored currently, rather than the in-progress update.
  //
  // TODO(lazyboy): Consider updating callers to directly retrieve version()
  // from either GetExtensionById() or querying ExtensionSet getters of this
  // class.
  base::Version GetStoredVersion(const ExtensionId& id) const;

  // The usual observer interface.
  void AddObserver(ExtensionRegistryObserver* observer);
  void RemoveObserver(ExtensionRegistryObserver* observer);

  // Invokes the observer method OnExtensionLoaded(). The extension must be
  // enabled at the time of the call.
  void TriggerOnLoaded(const Extension* extension);

  // Invokes the observer method OnExtensionReady(). This always follows
  // an OnLoaded event, but is not called until it's safe to create the
  // extension's child process.
  void TriggerOnReady(const Extension* extension);

  // Invokes the observer method OnExtensionUnloaded(). The extension must not
  // be enabled at the time of the call.
  void TriggerOnUnloaded(const Extension* extension,
                         UnloadedExtensionReason reason);

  // If this is a fresh install then |is_update| is false and there must not be
  // any installed extension with |extension|'s ID. If this is an update then
  // |is_update| is true and must be an installed extension with |extension|'s
  // ID, and |old_name| must be non-empty.
  void TriggerOnWillBeInstalled(const Extension* extension,
                                bool is_update,
                                const std::string& old_name);

  // Invokes the observer method OnExtensionInstalled(). The extension must be
  // contained in one of the registry's extension sets.
  void TriggerOnInstalled(const Extension* extension,
                          bool is_update);

  // Invokes the observer method OnExtensionUninstalled(). The extension must
  // not be any installed extension with |extension|'s ID.
  void TriggerOnUninstalled(const Extension* extension, UninstallReason reason);

  // Invokes the observer method OnExtensionUninstallationDenied().
  void TriggerOnUninstallationDenied(const Extension* extension);

  // Find an extension by ID using |include_mask| to pick the sets to search:
  //  * enabled_extensions()     --> ExtensionRegistry::ENABLED
  //  * disabled_extensions()    --> ExtensionRegistry::DISABLED
  //  * terminated_extensions()  --> ExtensionRegistry::TERMINATED
  //  * blocklisted_extensions() --> ExtensionRegistry::BLOCKLISTED
  //  * blocked_extensions()     --> ExtensionRegistry::BLOCKED
  // Returns NULL if the extension is not found in the selected sets.
  const Extension* GetExtensionById(const std::string& id,
                                    int include_mask) const;

  // Looks up an extension by ID, regardless of whether it's enabled,
  // disabled, blocklisted, or terminated.
  const Extension* GetInstalledExtension(const std::string& id) const;

  // Adds the specified extension to the enabled set. The registry becomes an
  // owner. Any previous extension with the same ID is removed.
  // Returns true if there is no previous extension.
  // NOTE: You probably want to use ExtensionService instead of calling this
  // method directly.
  bool AddEnabled(const scoped_refptr<const Extension>& extension);

  // Removes the specified extension from the enabled set.
  // Returns true if the set contained the specified extension.
  // NOTE: You probably want to use ExtensionService instead of calling this
  // method directly.
  bool RemoveEnabled(const std::string& id);

  // As above, but for the disabled set.
  bool AddDisabled(const scoped_refptr<const Extension>& extension);
  bool RemoveDisabled(const std::string& id);

  // As above, but for the terminated set.
  bool AddTerminated(const scoped_refptr<const Extension>& extension);
  bool RemoveTerminated(const std::string& id);

  // As above, but for the blocklisted set.
  bool AddBlocklisted(const scoped_refptr<const Extension>& extension);
  bool RemoveBlocklisted(const std::string& id);

  // As above, but for the blocked set.
  bool AddBlocked(const scoped_refptr<const Extension>& extension);
  bool RemoveBlocked(const std::string& id);

  // As above, but for the ready set.
  bool AddReady(const scoped_refptr<const Extension>& extension);
  bool RemoveReady(const std::string& id);

  // Removes all extensions from all sets.
  void ClearAll();

  // KeyedService implementation:
  void Shutdown() override;

 private:
  // Extensions that are installed, enabled and not terminated.
  ExtensionSet enabled_extensions_;

  // Extensions that are installed and disabled.
  ExtensionSet disabled_extensions_;

  // Extensions that are installed and terminated.
  ExtensionSet terminated_extensions_;

  // Extensions that are installed and blocklisted. Generally these shouldn't be
  // considered as installed by the extension platform: we only keep them around
  // so that if extensions are blocklisted by mistake they can easily be
  // un-blocklisted.
  ExtensionSet blocklisted_extensions_;

  // Extensions that are installed and blocked. Will never be loaded.
  ExtensionSet blocked_extensions_;

  // Extensions that are ready for execution. This set is a non-exclusive
  // subset of |enabled_extensions_|.
  ExtensionSet ready_extensions_;

  base::ObserverList<ExtensionRegistryObserver>::UncheckedAndDanglingUntriaged
      observers_;

  const raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRY_H_
