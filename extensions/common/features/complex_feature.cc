// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"

namespace extensions {

const ComplexFeatureData& ComplexFeature::descriptor() const {
  return *reinterpret_cast<const ComplexFeatureData*>(feature_data_);
}

bool ComplexFeature::VisitFeatures(
    base::FunctionRef<bool(const Feature&)> visitor) const {
  for (const auto& data : descriptor().features.span()) {
    bool should_continue = false;
    switch (descriptor().feature_type) {
      case ComplexFeatureType::kSimple: {
        SimpleFeature feature(&data);
        should_continue = visitor(feature);
        break;
      }
      case ComplexFeatureType::kManifest: {
        ManifestFeature feature(&data);
        should_continue = visitor(feature);
        break;
      }
      case ComplexFeatureType::kPermission: {
        PermissionFeature feature(&data);
        should_continue = visitor(feature);
        break;
      }
    }
    if (!should_continue) {
      return false;
    }
  }
  return true;
}

Feature::Availability ComplexFeature::FindFirstAvailability(
    base::FunctionRef<Availability(const Feature&)> get_availability) const {
  std::optional<Availability> result;
  VisitFeatures([&](const Feature& feature) {
    Availability availability = get_availability(feature);
    const bool is_available = availability.is_available();
    // Keep the first result, then upgrade to the first available one.
    if (!result || is_available) {
      result = std::move(availability);
    }
    return !is_available;
  });
  CHECK(result);
  return std::move(*result);
}

Feature::Availability ComplexFeature::IsAvailableToManifest(
    const HashedExtensionId& hashed_id,
    Manifest::Type type,
    mojom::ManifestLocation location,
    int manifest_version,
    Platform platform,
    int context_id) const {
  return FindFirstAvailability([&](const Feature& feature) {
    return feature.IsAvailableToManifest(
        hashed_id, type, location, manifest_version, platform, context_id);
  });
}

Feature::Availability ComplexFeature::IsAvailableToContextImpl(
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    Platform platform,
    int context_id,
    bool check_developer_mode,
    const ContextData& context_data,
    DelegatedAvailabilityCheckHandler delegated_handler) const {
  const DelegatedAvailabilityCheckHandler installed_handler =
      ResolveDelegatedAvailabilityCheckHandler(delegated_handler);
  return FindFirstAvailability([&](const Feature& feature) {
    return feature.IsAvailableToContextImpl(extension, context, url, platform,
                                            context_id, check_developer_mode,
                                            context_data, installed_handler);
  });
}

Feature::Availability ComplexFeature::IsAvailableToEnvironment(
    int context_id) const {
  return FindFirstAvailability([&](const Feature& feature) {
    return feature.IsAvailableToEnvironment(context_id);
  });
}

bool ComplexFeature::IsIdInBlocklist(const HashedExtensionId& hashed_id) const {
  bool found = false;
  VisitFeatures([&](const Feature& feature) {
    found = feature.IsIdInBlocklist(hashed_id);
    return !found;
  });
  return found;
}

bool ComplexFeature::IsIdInAllowlist(const HashedExtensionId& hashed_id) const {
  bool found = false;
  VisitFeatures([&](const Feature& feature) {
    found = feature.IsIdInAllowlist(hashed_id);
    return !found;
  });
  return found;
}

bool ComplexFeature::IsInternal() const {
  // Compile-time descriptor validation guarantees that composed features are
  // consistent, so the first feature's value represents them all.
  return descriptor().features.span().front().config.is_internal;
}

bool ComplexFeature::RequiresDelegatedAvailabilityCheck() const {
  // Derived from the children rather than cached, so that this feature holds
  // no state of its own beyond the descriptor pointer. The child count is
  // small and the descriptors are contiguous.
  for (const auto& feature : descriptor().features.span()) {
    if (feature.config.requires_delegated_availability_check) {
      return true;
    }
  }
  return false;
}

}  // namespace extensions
