// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/shared_module.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace keys = manifest_keys;

namespace {

// Rank extension locations in a way that allows
// Manifest::GetHigherPriorityLocation() to compare locations.
// An extension installed from two locations will have the location
// with the higher rank, as returned by this function. The actual
// integer values may change, and should never be persisted.
int GetLocationRank(ManifestLocation location) {
  const int kInvalidRank = -1;
  int rank = kInvalidRank;  // Will CHECK that rank is not kInvalidRank.

  switch (location) {
    // Component extensions can not be overridden by any other type.
    case ManifestLocation::kComponent:
      rank = 9;
      break;

    case ManifestLocation::kExternalComponent:
      rank = 8;
      break;

    // Policy controlled extensions may not be overridden by any type
    // that is not part of chrome.
    case ManifestLocation::kExternalPolicy:
      rank = 7;
      break;

    case ManifestLocation::kExternalPolicyDownload:
      rank = 6;
      break;

    // A developer-loaded extension should override any installed type
    // that a user can disable. Anything specified on the command-line should
    // override one loaded via the extensions UI.
    case ManifestLocation::kCommandLine:
      rank = 5;
      break;

    case ManifestLocation::kUnpacked:
      rank = 4;
      break;

    // The relative priority of various external sources is not important,
    // but having some order ensures deterministic behavior.
    case ManifestLocation::kExternalRegistry:
      rank = 3;
      break;

    case ManifestLocation::kExternalPref:
      rank = 2;
      break;

    case ManifestLocation::kExternalPrefDownload:
      rank = 1;
      break;

    // User installed extensions are overridden by any external type.
    case ManifestLocation::kInternal:
      rank = 0;
      break;

    // kInvalidLocation should never be passed to this function.
    case ManifestLocation::kInvalidLocation:
      break;
  }

  CHECK(rank != kInvalidRank);
  return rank;
}

int GetManifestVersion(const base::DictionaryValue& manifest_value,
                       Manifest::Type type) {
  // Platform apps were launched after manifest version 2 was the preferred
  // version, so they default to that.
  int manifest_version = type == Manifest::TYPE_PLATFORM_APP ? 2 : 1;
  manifest_value.GetInteger(keys::kManifestVersion, &manifest_version);
  return manifest_version;
}

// Helper class to filter available values from a manifest.
class AvailableValuesFilter {
 public:
  // Filters `manifest.values()` removing any unavailable keys.
  static base::Value Filter(const Manifest& manifest) {
    return FilterInternal(manifest, *manifest.value(), "");
  }

 private:
  // Returns a DictionaryValue corresponding to |input_dict| for the given
  // |manifest|, with all unavailable keys removed.
  static base::Value FilterInternal(const Manifest& manifest,
                                    const base::Value& input_dict,
                                    std::string current_path) {
    base::Value output_dict(base::Value::Type::DICTIONARY);
    DCHECK(input_dict.is_dict());
    DCHECK(CanAccessFeature(manifest, current_path));

    for (auto it : input_dict.DictItems()) {
      std::string child_path = CombineKeys(current_path, it.first);

      // Unavailable key, skip it.
      if (!CanAccessFeature(manifest, child_path))
        continue;

      // If |child_path| corresponds to a leaf node, copy it.
      bool is_leaf_node = !it.second.is_dict();
      if (is_leaf_node) {
        output_dict.SetKey(it.first, it.second.Clone());
        continue;
      }

      // Child dictionary. Populate it recursively.
      output_dict.SetKey(it.first,
                         FilterInternal(manifest, it.second, child_path));
    }
    return output_dict;
  }

  // Returns true if the manifest feature corresponding to |feature_path| is
  // available to this manifest. Note: This doesn't check parent feature
  // availability. This is ok since we check feature availability in a
  // breadth-first manner below which ensures that we only ever check a child
  // feature if its parent is available. Note that api features don't follow
  // similar availability semantics i.e. we can have child api features be
  // available even if the parent feature is not (e.g.,
  // runtime.sendMessage()).
  static bool CanAccessFeature(const Manifest& manifest,
                               const std::string& feature_path) {
    const Feature* feature =
        FeatureProvider::GetManifestFeatures()->GetFeature(feature_path);

    // TODO(crbug.com/1171466): We assume that if a feature does not exist,
    // it is available. This is ok for child features (if its parent is
    // available) but is probably not correct for top-level features. We
    // should see if false can be returned for these non-existent top-level
    // features here.
    if (!feature)
      return true;

    return feature
        ->IsAvailableToManifest(manifest.hashed_id(), manifest.type(),
                                manifest.location(),
                                manifest.manifest_version())
        .is_available();
  }

  static std::string CombineKeys(const std::string& parent,
                                 const std::string& child) {
    if (parent.empty())
      return child;

    return base::StrCat({parent, ".", child});
  }
};

}  // namespace

// static
ManifestLocation Manifest::GetHigherPriorityLocation(ManifestLocation loc1,
                                                     ManifestLocation loc2) {
  if (loc1 == loc2)
    return loc1;

  int loc1_rank = GetLocationRank(loc1);
  int loc2_rank = GetLocationRank(loc2);

  // If two different locations have the same rank, then we can not
  // deterministicly choose a location.
  CHECK(loc1_rank != loc2_rank);

  // Highest rank has highest priority.
  return (loc1_rank > loc2_rank ? loc1 : loc2 );
}

// static
Manifest::Type Manifest::GetTypeFromManifestValue(
    const base::DictionaryValue& value,
    bool for_login_screen) {
  Type type = TYPE_UNKNOWN;
  if (value.HasKey(keys::kTheme)) {
    type = TYPE_THEME;
  } else if (value.HasKey(api::shared_module::ManifestKeys::kExport)) {
    type = TYPE_SHARED_MODULE;
  } else if (value.HasKey(keys::kApp)) {
    if (value.Get(keys::kWebURLs, nullptr) ||
        value.Get(keys::kLaunchWebURL, nullptr)) {
      type = TYPE_HOSTED_APP;
    } else if (value.Get(keys::kPlatformAppBackground, nullptr)) {
      type = TYPE_PLATFORM_APP;
    } else {
      type = TYPE_LEGACY_PACKAGED_APP;
    }
  } else if (value.HasKey(keys::kChromeOSSystemExtension)) {
    type = TYPE_CHROMEOS_SYSTEM_EXTENSION;
  } else if (for_login_screen) {
    type = TYPE_LOGIN_SCREEN_EXTENSION;
  } else {
    type = TYPE_EXTENSION;
  }
  DCHECK_NE(type, TYPE_UNKNOWN);

  return type;
}

// static
bool Manifest::ShouldAlwaysLoadExtension(ManifestLocation location,
                                         bool is_theme) {
  if (location == ManifestLocation::kComponent)
    return true;  // Component extensions are always allowed.

  if (is_theme)
    return true;  // Themes are allowed, even with --disable-extensions.

  // TODO(devlin): This seems wrong. See https://crbug.com/833540.
  if (Manifest::IsExternalLocation(location))
    return true;

  return false;
}

// static
std::unique_ptr<Manifest> Manifest::CreateManifestForLoginScreen(
    ManifestLocation location,
    std::unique_ptr<base::DictionaryValue> value,
    ExtensionId extension_id) {
  CHECK(IsPolicyLocation(location));
  // Use base::WrapUnique + new because the constructor is private.
  return base::WrapUnique(
      new Manifest(location, std::move(value), std::move(extension_id), true));
}

Manifest::Manifest(ManifestLocation location,
                   std::unique_ptr<base::DictionaryValue> value,
                   ExtensionId extension_id)
    : Manifest(location, std::move(value), std::move(extension_id), false) {}

Manifest::Manifest(ManifestLocation location,
                   std::unique_ptr<base::DictionaryValue> value,
                   ExtensionId extension_id,
                   bool for_login_screen)
    : extension_id_(std::move(extension_id)),
      hashed_id_(HashedExtensionId(extension_id_)),
      location_(location),
      value_(std::move(value)),
      type_(GetTypeFromManifestValue(*value_, for_login_screen)),
      manifest_version_(GetManifestVersion(*value_, type_)) {
  DCHECK(!extension_id_.empty());

  available_values_ = base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(AvailableValuesFilter::Filter(*this)));
}

Manifest::~Manifest() = default;

bool Manifest::ValidateManifest(
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  *error = "";

  // Check every feature to see if its in the manifest. Note that this means
  // we will ignore keys that are not features; we do this for forward
  // compatibility.

  const FeatureProvider* manifest_feature_provider =
      FeatureProvider::GetManifestFeatures();
  for (const auto& map_entry : manifest_feature_provider->GetAllFeatures()) {
    // Use Get instead of HasKey because the former uses path expansion.
    if (!value_->Get(map_entry.first, nullptr))
      continue;

    Feature::Availability result = map_entry.second->IsAvailableToManifest(
        hashed_id_, type_, location_, manifest_version_);
    if (!result.is_available())
      warnings->push_back(InstallWarning(result.message(), map_entry.first));
  }

  // Also generate warnings for keys that are not features.
  for (base::DictionaryValue::Iterator it(*value_); !it.IsAtEnd();
       it.Advance()) {
    if (!manifest_feature_provider->GetFeature(it.key())) {
      warnings->push_back(InstallWarning(
          ErrorUtils::FormatErrorMessage(
              manifest_errors::kUnrecognizedManifestKey, it.key()),
          it.key()));
    }
  }

  if (IsUnpackedLocation(location_) &&
      value_->FindPath(manifest_keys::kDifferentialFingerprint)) {
    warnings->push_back(
        InstallWarning(manifest_errors::kHasDifferentialFingerprint,
                       manifest_keys::kDifferentialFingerprint));
  }
  return true;
}

bool Manifest::HasKey(const std::string& key) const {
  return available_values_->HasKey(key);
}

bool Manifest::HasPath(const std::string& path) const {
  const base::Value* ignored = nullptr;
  return available_values_->Get(path, &ignored);
}

bool Manifest::Get(
    const std::string& path, const base::Value** out_value) const {
  return available_values_->Get(path, out_value);
}

bool Manifest::GetBoolean(
    const std::string& path, bool* out_value) const {
  return available_values_->GetBoolean(path, out_value);
}

bool Manifest::GetInteger(
    const std::string& path, int* out_value) const {
  return available_values_->GetInteger(path, out_value);
}

bool Manifest::GetString(
    const std::string& path, std::string* out_value) const {
  return available_values_->GetString(path, out_value);
}

bool Manifest::GetString(const std::string& path,
                         std::u16string* out_value) const {
  return available_values_->GetString(path, out_value);
}

bool Manifest::GetDictionary(
    const std::string& path, const base::DictionaryValue** out_value) const {
  return available_values_->GetDictionary(path, out_value);
}

bool Manifest::GetDictionary(const std::string& path,
                             const base::Value** out_value) const {
  return GetPathOfType(path, base::Value::Type::DICTIONARY, out_value);
}

bool Manifest::GetList(
    const std::string& path, const base::ListValue** out_value) const {
  return available_values_->GetList(path, out_value);
}

bool Manifest::GetList(const std::string& path,
                       const base::Value** out_value) const {
  return GetPathOfType(path, base::Value::Type::LIST, out_value);
}

bool Manifest::GetPathOfType(const std::string& path,
                             base::Value::Type type,
                             const base::Value** out_value) const {
  const std::vector<base::StringPiece> components =
      manifest_handler_helpers::TokenizeDictionaryPath(path);
  *out_value = available_values_->FindPathOfType(components, type);
  return *out_value != nullptr;
}

bool Manifest::EqualsForTesting(const Manifest& other) const {
  return *value_ == *other.value() && location_ == other.location_ &&
         extension_id_ == other.extension_id_;
}

}  // namespace extensions
