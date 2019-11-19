// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"

namespace extensions {

namespace keys = manifest_keys;

namespace {

// Rank extension locations in a way that allows
// Manifest::GetHigherPriorityLocation() to compare locations.
// An extension installed from two locations will have the location
// with the higher rank, as returned by this function. The actual
// integer values may change, and should never be persisted.
int GetLocationRank(Manifest::Location location) {
  const int kInvalidRank = -1;
  int rank = kInvalidRank;  // Will CHECK that rank is not kInvalidRank.

  switch (location) {
    // Component extensions can not be overriden by any other type.
    case Manifest::COMPONENT:
      rank = 9;
      break;

    case Manifest::EXTERNAL_COMPONENT:
      rank = 8;
      break;

    // Policy controlled extensions may not be overridden by any type
    // that is not part of chrome.
    case Manifest::EXTERNAL_POLICY:
      rank = 7;
      break;

    case Manifest::EXTERNAL_POLICY_DOWNLOAD:
      rank = 6;
      break;

    // A developer-loaded extension should override any installed type
    // that a user can disable. Anything specified on the command-line should
    // override one loaded via the extensions UI.
    case Manifest::COMMAND_LINE:
      rank = 5;
      break;

    case Manifest::UNPACKED:
      rank = 4;
      break;

    // The relative priority of various external sources is not important,
    // but having some order ensures deterministic behavior.
    case Manifest::EXTERNAL_REGISTRY:
      rank = 3;
      break;

    case Manifest::EXTERNAL_PREF:
      rank = 2;
      break;

    case Manifest::EXTERNAL_PREF_DOWNLOAD:
      rank = 1;
      break;

    // User installed extensions are overridden by any external type.
    case Manifest::INTERNAL:
      rank = 0;
      break;

    default:
      NOTREACHED() << "Need to add new extension location " << location;
  }

  CHECK(rank != kInvalidRank);
  return rank;
}

}  // namespace

// static
Manifest::Location Manifest::GetHigherPriorityLocation(
    Location loc1, Location loc2) {
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
  } else if (value.HasKey(keys::kExport)) {
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
  } else if (for_login_screen) {
    type = TYPE_LOGIN_SCREEN_EXTENSION;
  } else {
    type = TYPE_EXTENSION;
  }
  DCHECK_NE(type, TYPE_UNKNOWN);

  return type;
}

// static
bool Manifest::ShouldAlwaysLoadExtension(Manifest::Location location,
                                         bool is_theme) {
  if (location == Manifest::COMPONENT)
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
    Location location,
    std::unique_ptr<base::DictionaryValue> value) {
  CHECK(IsPolicyLocation(location));
  // Use base::WrapUnique + new because the constructor is private.
  return base::WrapUnique(new Manifest(location, std::move(value), true));
}

Manifest::Manifest(Location location,
                   std::unique_ptr<base::DictionaryValue> value)
    : Manifest(location, std::move(value), false) {}

Manifest::Manifest(Location location,
                   std::unique_ptr<base::DictionaryValue> value,
                   bool for_login_screen)
    : location_(location),
      value_(std::move(value)),
      type_(GetTypeFromManifestValue(*value_, for_login_screen)) {}

Manifest::~Manifest() {
}

void Manifest::SetExtensionId(const ExtensionId& id) {
  extension_id_ = id;
  hashed_id_ = HashedExtensionId(id);
}

bool Manifest::ValidateManifest(
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  *error = "";

  // Check every feature to see if its in the manifest. Note that this means
  // we will ignore keys that are not features; we do this for forward
  // compatibility.
  // TODO(aa): Consider having an error here in the case of strict error
  // checking to let developers know when they screw up.

  const FeatureProvider* manifest_feature_provider =
      FeatureProvider::GetManifestFeatures();
  for (const auto& map_entry : manifest_feature_provider->GetAllFeatures()) {
    // Use Get instead of HasKey because the former uses path expansion.
    if (!value_->Get(map_entry.first, nullptr))
      continue;

    Feature::Availability result = map_entry.second->IsAvailableToManifest(
        hashed_id_, type_, location_, GetManifestVersion());
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
  return CanAccessKey(key) && value_->HasKey(key);
}

bool Manifest::HasPath(const std::string& path) const {
  base::Value* ignored = NULL;
  return CanAccessPath(path) && value_->Get(path, &ignored);
}

bool Manifest::Get(
    const std::string& path, const base::Value** out_value) const {
  return CanAccessPath(path) && value_->Get(path, out_value);
}

bool Manifest::GetBoolean(
    const std::string& path, bool* out_value) const {
  return CanAccessPath(path) && value_->GetBoolean(path, out_value);
}

bool Manifest::GetInteger(
    const std::string& path, int* out_value) const {
  return CanAccessPath(path) && value_->GetInteger(path, out_value);
}

bool Manifest::GetString(
    const std::string& path, std::string* out_value) const {
  return CanAccessPath(path) && value_->GetString(path, out_value);
}

bool Manifest::GetString(
    const std::string& path, base::string16* out_value) const {
  return CanAccessPath(path) && value_->GetString(path, out_value);
}

bool Manifest::GetDictionary(
    const std::string& path, const base::DictionaryValue** out_value) const {
  return CanAccessPath(path) && value_->GetDictionary(path, out_value);
}

bool Manifest::GetDictionary(const std::string& path,
                             const base::Value** out_value) const {
  return GetPathOfType(path, base::Value::Type::DICTIONARY, out_value);
}

bool Manifest::GetList(
    const std::string& path, const base::ListValue** out_value) const {
  return CanAccessPath(path) && value_->GetList(path, out_value);
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
  if (!CanAccessPath(components))
    return false;
  *out_value = value_->FindPathOfType(components, type);
  return *out_value != nullptr;
}

std::unique_ptr<Manifest> Manifest::CreateDeepCopy() const {
  auto manifest =
      std::make_unique<Manifest>(location_, value_->CreateDeepCopy());
  manifest->SetExtensionId(extension_id_);
  return manifest;
}

bool Manifest::Equals(const Manifest* other) const {
  return other && value_->Equals(other->value());
}

int Manifest::GetManifestVersion() const {
  // Platform apps were launched after manifest version 2 was the preferred
  // version, so they default to that.
  int manifest_version = type_ == TYPE_PLATFORM_APP ? 2 : 1;
  value_->GetInteger(keys::kManifestVersion, &manifest_version);
  return manifest_version;
}

bool Manifest::CanAccessPath(const std::string& path) const {
  return CanAccessPath(manifest_handler_helpers::TokenizeDictionaryPath(path));
}

bool Manifest::CanAccessPath(base::span<const base::StringPiece> path) const {
  std::string key;
  for (base::StringPiece component : path) {
    component.AppendToString(&key);
    if (!CanAccessKey(key))
      return false;
    key += '.';
  }
  return true;
}

bool Manifest::CanAccessKey(const std::string& key) const {
  const Feature* feature =
      FeatureProvider::GetManifestFeatures()->GetFeature(key);
  if (!feature)
    return true;

  return feature
      ->IsAvailableToManifest(hashed_id_, type_, location_,
                              GetManifestVersion())
      .is_available();
}

}  // namespace extensions
