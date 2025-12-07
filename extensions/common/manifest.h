// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_H_
#define EXTENSIONS_COMMON_MANIFEST_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/hashed_extension_id.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace extensions {
struct InstallWarning;

// Wraps the base::Value::Dict form of extension's manifest. Enforces access to
// properties of the manifest using ManifestFeatureProvider.
class Manifest final {
 public:
  // Do not change the order of entries or remove entries in this list as this
  // is used in ExtensionType enum in
  // tools/metrics/histograms/metadata/extensions/enums.xml.
  // TODO(crbug.com/420858216): Add add "class" to declaration.
  enum Type {
    kUnknown = 0,
    kExtension = 1,
    kTheme = 2,
    kUserScript = 3,
    kHostedApp = 4,
    // This is marked legacy because platform apps are preferred. For
    // backwards compatibility, we can't remove support for packaged apps
    kLegacyPackagedApp = 5,
    kPlatformApp = 6,
    kSharedModule = 7,
    kLoginScreenExtension = 8,
    kChromeOSSystemExtension = 9,

    // New enum values must go above here.
    kNumLoadTypes,

    // TODO(crbug.com/420858216): Remove these legacy values/names.
    TYPE_UNKNOWN = kUnknown,
    TYPE_EXTENSION = kExtension,
    TYPE_THEME = kTheme,
    TYPE_USER_SCRIPT = kUserScript,
    TYPE_HOSTED_APP = kHostedApp,
    TYPE_LEGACY_PACKAGED_APP = kLegacyPackagedApp,
    TYPE_PLATFORM_APP = kPlatformApp,
    TYPE_SHARED_MODULE = kSharedModule,
    TYPE_LOGIN_SCREEN_EXTENSION = kLoginScreenExtension,
    TYPE_CHROMEOS_SYSTEM_EXTENSION = kChromeOSSystemExtension,
    NUM_LOAD_TYPES = kNumLoadTypes
  };

  // Given two install sources, return the one which should take priority
  // over the other. If an extension is installed from two sources A and B,
  // its install source should be set to GetHigherPriorityLocation(A, B).
  static mojom::ManifestLocation GetHigherPriorityLocation(
      mojom::ManifestLocation loc1,
      mojom::ManifestLocation loc2);

  // Whether the `location` is external or not.
  static inline bool IsExternalLocation(mojom::ManifestLocation location) {
    return location == mojom::ManifestLocation::kExternalPref ||
           location == mojom::ManifestLocation::kExternalRegistry ||
           location == mojom::ManifestLocation::kExternalPrefDownload ||
           location == mojom::ManifestLocation::kExternalPolicy ||
           location == mojom::ManifestLocation::kExternalPolicyDownload ||
           location == mojom::ManifestLocation::kExternalComponent;
  }

  // Whether the `location` is unpacked (no CRX) or not.
  static inline bool IsUnpackedLocation(mojom::ManifestLocation location) {
    return location == mojom::ManifestLocation::kUnpacked ||
           location == mojom::ManifestLocation::kCommandLine;
  }

  // Whether extensions with `location` are auto-updatable or not.
  static inline bool IsAutoUpdateableLocation(
      mojom::ManifestLocation location) {
    // Only internal and external extensions can be autoupdated.
    return location == mojom::ManifestLocation::kInternal ||
           IsExternalLocation(location);
  }

  // Whether the `location` is a source of extensions force-installed through
  // policy.
  static inline bool IsPolicyLocation(mojom::ManifestLocation location) {
    return location == mojom::ManifestLocation::kExternalPolicy ||
           location == mojom::ManifestLocation::kExternalPolicyDownload;
  }

  // Whether the `location` is an extension intended to be an internal part of
  // Chrome.
  static inline bool IsComponentLocation(mojom::ManifestLocation location) {
    return location == mojom::ManifestLocation::kComponent ||
           location == mojom::ManifestLocation::kExternalComponent;
  }

  static inline bool IsValidLocation(mojom::ManifestLocation location) {
    return location > mojom::ManifestLocation::kInvalidLocation &&
           location <= mojom::ManifestLocation::kMaxValue;
  }

  // Unpacked extensions start off with file access since they are a developer
  // feature.
  static inline bool ShouldAlwaysAllowFileAccess(
      mojom::ManifestLocation location) {
    return IsUnpackedLocation(location);
  }

  // Returns the Manifest::Type for the given `value`.
  static Type GetTypeFromManifestValue(const base::Value::Dict& value,
                                       bool for_login_screen = false);

  // Returns true if an item with the given `location` should always be loaded,
  // even if extensions are otherwise disabled.
  static bool ShouldAlwaysLoadExtension(mojom::ManifestLocation location,
                                        bool is_theme);

  // Creates a Manifest for a login screen context. Note that this won't always
  // result in a Manifest of Type::kLoginScreenExtension, since other items
  // (like platform apps) may be installed in the same login screen profile.
  static std::unique_ptr<Manifest> CreateManifestForLoginScreen(
      mojom::ManifestLocation location,
      base::Value::Dict value,
      ExtensionId extension_id);

  Manifest(mojom::ManifestLocation location,
           base::Value::Dict value,
           ExtensionId extension_id);

  Manifest(const Manifest&) = delete;
  Manifest& operator=(const Manifest&) = delete;

  ~Manifest();

  const ExtensionId& extension_id() const { return extension_id_; }
  const HashedExtensionId& hashed_id() const { return hashed_id_; }

  mojom::ManifestLocation location() const { return location_; }

  // Populates `warnings` if manifest contains keys not permitted for the
  // chosen extension type.
  void ValidateManifest(std::vector<InstallWarning>* warnings) const;

  // The version of this extension's manifest. We increase the manifest
  // version when making breaking changes to the extension system. If the
  // manifest contains no explicit manifest version, this returns the current
  // system default.
  int manifest_version() const { return manifest_version_; }

  // Returns the manifest type.
  Type type() const { return type_; }

  bool is_theme() const { return type_ == Type::kTheme; }
  bool is_app() const {
    return is_legacy_packaged_app() || is_hosted_app() || is_platform_app();
  }
  bool is_platform_app() const { return type_ == Type::kPlatformApp; }
  bool is_hosted_app() const { return type_ == Type::kHostedApp; }
  bool is_legacy_packaged_app() const {
    return type_ == Type::kLegacyPackagedApp;
  }
  bool is_extension() const { return type_ == Type::kExtension; }
  bool is_login_screen_extension() const {
    return type_ == Type::kLoginScreenExtension;
  }
  bool is_shared_module() const { return type_ == Type::kSharedModule; }
  bool is_chromeos_system_extension() const {
    return type_ == Type::kChromeOSSystemExtension;
  }

  // These access the wrapped manifest value, returning nullptr/nullopt when the
  // property does not exist or if the manifest type can't access it.
  const base::Value* FindKey(std::string_view path) const;
  const base::Value* FindPath(std::string_view path) const;
  std::optional<bool> FindBoolPath(std::string_view path) const;
  std::optional<int> FindIntPath(std::string_view path) const;
  const std::string* FindStringPath(std::string_view path) const;

  const base::Value::Dict* FindDictPath(std::string_view path) const;

  // Deprecated: Use the FindDictPath(asValue) functions instead.
  bool GetList(const std::string& path, const base::Value** out_value) const;

  // Returns true if this equals the `other` manifest.
  bool EqualsForTesting(const Manifest& other) const;

  // Gets the underlying base::Value::Dict representing the manifest.
  // Note: only use this when you KNOW you don't need the validation.
  const base::Value::Dict* value() const { return &value_; }

  // Gets the underlying `base::Value::Dict` representing the manifest with all
  // unavailable manifest keys removed.
  const base::Value::Dict& available_values() const {
    return available_values_;
  }

 private:
  Manifest(mojom::ManifestLocation location,
           base::Value::Dict value,
           ExtensionId extension_id,
           bool for_login_screen);

  // A persistent, globally unique ID. An extension's ID is used in things
  // like directory structures and URLs, and is expected to not change across
  // versions. It is generated as a SHA-256 hash of the extension's public
  // key, or as a hash of the path in the case of unpacked extensions.
  const ExtensionId extension_id_;

  // The hex-encoding of the SHA1 of the extension id; used to determine feature
  // availability.
  const HashedExtensionId hashed_id_;

  // The location the extension was loaded from.
  const mojom::ManifestLocation location_;

  // The underlying dictionary representation of the manifest.
  const base::Value::Dict value_;

  // Same as `value_` but comprises only of keys available to this manifest.
  base::Value::Dict available_values_;

  const Type type_;

  const int manifest_version_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_H_
