// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLER_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/small_map.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "extensions/common/manifest.h"

namespace extensions {
class Extension;
class ManifestPermission;
class ManifestPermissionSet;

// An interface for clients that recognize and parse keys in extension
// manifests.
class ManifestHandler {
 public:
  ManifestHandler();
  virtual ~ManifestHandler();

  // Attempts to parse the extension's manifest.
  // Returns true on success or false on failure; if false, |error| will
  // be set to a failure message.
  // This does not perform any IO operations.
  virtual bool Parse(Extension* extension, base::string16* error) = 0;

  // Validate that files associated with this manifest key exist.
  // Validation takes place after parsing. May also append a series of
  // warning messages to |warnings|.
  // This may perform IO operations.
  //
  // Otherwise, returns false, and a description of the error is
  // returned in |error|.
  // TODO(yoz): Change error to base::string16. See crbug.com/71980.
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const;

  // If false (the default), only parse the manifest if a registered
  // key is present in the manifest. If true, always attempt to parse
  // the manifest for this extension type, even if no registered keys
  // are present. This allows specifying a default parsed value for
  // extensions that don't declare our key in the manifest.
  // TODO(yoz): Use Feature availability instead.
  virtual bool AlwaysParseForType(Manifest::Type type) const;

  // Same as AlwaysParseForType, but for Validate instead of Parse.
  virtual bool AlwaysValidateForType(Manifest::Type type) const;

  // The list of keys that, if present, should be parsed before calling our
  // Parse (typically, because our Parse needs to read those keys).
  // Defaults to empty.
  virtual const std::vector<std::string> PrerequisiteKeys() const;

  // Creates a |ManifestPermission| instance for the given manifest key |name|.
  // The returned permission does not contain any permission data, so this
  // method is usually used before calling |FromValue| or |Read|. Returns
  // |NULL| if the manifest handler does not support custom permissions.
  virtual ManifestPermission* CreatePermission();

  // Creates a |ManifestPermission| instance containing the initial set of
  // required manifest permissions for the given |extension|. Returns |NULL| if
  // the manifest handler does not support custom permissions or if there was
  // no manifest key in the extension manifest for this handler.
  virtual ManifestPermission* CreateInitialRequiredPermission(
      const Extension* extension);

  // The keys this handler is responsible for.
  virtual base::span<const char* const> Keys() const = 0;

  // Calling FinalizeRegistration indicates that there are no more
  // manifest handlers to be registered.
  static void FinalizeRegistration();

  static bool IsRegistrationFinalized();

  // Call Parse on all registered manifest handlers that should parse
  // this extension.
  static bool ParseExtension(Extension* extension, base::string16* error);

  // Call Validate on all registered manifest handlers for this extension. This
  // may perform IO operations.
  static bool ValidateExtension(const Extension* extension,
                                std::string* error,
                                std::vector<InstallWarning>* warnings);

  // Calls |CreatePermission| on the manifest handler for |key|. Returns |NULL|
  // if there is no manifest handler for |key| or if the manifest handler for
  // |key| does not support custom permissions.
  static ManifestPermission* CreatePermission(const std::string& key);

  // Calls |CreateInitialRequiredPermission| on all registered manifest handlers
  // and adds the returned permissions to |permission_set|. Note this should be
  // called after all manifest data elements have been read, parsed and stored
  // in the manifest data property of |extension|, as manifest handlers need
  // access to their manifest data to initialize their required manifest
  // permission.
  static void AddExtensionInitialRequiredPermissions(
      const Extension* extension, ManifestPermissionSet* permission_set);

 protected:
  // A convenience method for handlers that only register for 1 key,
  // so that they can define keys() { return SingleKey(kKey); }
  static const std::vector<std::string> SingleKey(const std::string& key);

 private:
  DISALLOW_COPY_AND_ASSIGN(ManifestHandler);
};

// The global registry for manifest handlers.
class ManifestHandlerRegistry {
 public:
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

  bool ParseExtension(Extension* extension, base::string16* error);
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
  // to the backup std::unordered_map, which we don't want, as that would
  // defeat the optimization of using small_map.
  static constexpr size_t kHandlerMax = 75;
  using FallbackMap = std::unordered_map<std::string, ManifestHandler*>;
  using ManifestHandlerMap = base::small_map<FallbackMap, kHandlerMax>;
  using FallbackPriorityMap = std::unordered_map<ManifestHandler*, int>;
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

  bool is_finalized_;

  DISALLOW_COPY_AND_ASSIGN(ManifestHandlerRegistry);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLER_H_
