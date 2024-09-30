// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLER_REGISTRY_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLER_REGISTRY_H_

#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// The global registry for manifest handlers.
class ManifestHandlerRegistry {
 public:
  ManifestHandlerRegistry(const ManifestHandlerRegistry&) = delete;
  ManifestHandlerRegistry& operator=(const ManifestHandlerRegistry&) = delete;

  // Get the one true instance.
  static ManifestHandlerRegistry* Get();

  // Registers a ManifestHandler, associating it with its keys. If there is
  // already a handler registered for any key |handler| manages, this method
  // will DCHECK.
  void RegisterHandler(std::unique_ptr<ManifestHandler> handler);

 private:
  friend class ManifestHandler;
  friend class ScopedTestingManifestHandlerRegistry;
  friend struct base::LazyInstanceTraitsBase<ManifestHandlerRegistry>;
  FRIEND_TEST_ALL_PREFIXES(ManifestHandlerPerfTest, MANUAL_CommonInitialize);
  FRIEND_TEST_ALL_PREFIXES(ManifestHandlerPerfTest, MANUAL_LookupTest);
  FRIEND_TEST_ALL_PREFIXES(ManifestHandlerPerfTest,
                           MANUAL_CommonMeasureFinalization);
  FRIEND_TEST_ALL_PREFIXES(ChromeExtensionsClientTest,
                           CheckManifestHandlerRegistryForOverflow);

  ManifestHandlerRegistry();
  ~ManifestHandlerRegistry();

  void Finalize();

  bool ParseExtension(Extension* extension, std::u16string* error);
  bool ValidateExtension(const Extension* extension,
                         std::string* error,
                         std::vector<InstallWarning>* warnings);

  ManifestPermission* CreatePermission(const std::string& key);

  void AddExtensionInitialRequiredPermissions(
      const Extension* extension,
      ManifestPermissionSet* permission_set);

  // Reset the one true instance.
  static void ResetForTesting();

  // Overrides the current global ManifestHandlerRegistry with
  // |registry|, returning the current one.
  static ManifestHandlerRegistry* SetForTesting(
      ManifestHandlerRegistry* new_registry);

  // The owned collection of manifest handlers. These are then referenced by
  // raw pointer in maps for keys and priority.
  std::vector<std::unique_ptr<ManifestHandler>> owned_manifest_handlers_;

  // This number is derived from determining the total number of manifest
  // handlers that are installed for all build configurations. It is
  // checked through a unit test:
  // ChromeExtensionsClientTest.CheckManifestHandlerRegistryForOverflow.
  //
  // Any new manifest handlers added may cause the small_map to overflow
  // to the backup base::flat_map, which we don't want, as that would
  // defeat the optimization of using small_map.
  static constexpr size_t kHandlerMax = 87;
  using FallbackMap =
      base::flat_map<std::string, raw_ptr<ManifestHandler, CtnExperimental>>;
  using ManifestHandlerMap = base::small_map<FallbackMap, kHandlerMax>;
  using FallbackPriorityMap = base::flat_map<ManifestHandler*, int>;
  using ManifestHandlerPriorityMap =
      base::small_map<FallbackPriorityMap, kHandlerMax>;

  // Puts the manifest handlers in order such that each handler comes after
  // any handlers for their PrerequisiteKeys. If there is no handler for
  // a prerequisite key, that dependency is simply ignored.
  // CHECKs that there are no manifest handlers with circular dependencies.
  void SortManifestHandlers();

  // All registered manifest handlers.
  ManifestHandlerMap handlers_;

  // The priority for each manifest handler. Handlers with lower priority
  // values are evaluated first.
  ManifestHandlerPriorityMap priority_map_;

  bool is_finalized_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLER_REGISTRY_H_
