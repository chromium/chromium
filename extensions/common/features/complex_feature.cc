// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"

namespace extensions {

ComplexFeature::ComplexFeature(StaticFeatureData<ComplexFeatureData> data)
    : ComplexFeature(data.get()) {}

ComplexFeature::ComplexFeature(const ComplexFeatureData* data)
    : Feature(&data->feature), complex_feature_data_(data) {
  CHECK_GT(complex_feature_data_->features.span().size(), 1u);
  for (const auto& feature : complex_feature_data_->features.span()) {
    requires_delegated_availability_check_ |=
        feature.config.requires_delegated_availability_check;
  }

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  const auto features = complex_feature_data_->features.span();
  const bool first_is_internal = features.front().config.is_internal;
  for (const auto& feature : features) {
    DCHECK_EQ(first_is_internal, feature.config.is_internal)
        << "Complex feature must have consistent values of "
           "internal across all sub features.";
    DCHECK_EQ(no_parent(), feature.feature.no_parent)
        << "Complex feature must have consistent values of "
           "no_parent across all sub features.";
  }
#endif
}

ComplexFeature::~ComplexFeature() = default;

bool ComplexFeature::VisitFeatures(
    base::FunctionRef<bool(const Feature&)> visitor) const {
  for (const auto& data : complex_feature_data_->features.span()) {
    auto visit = [&](SimpleFeature& feature) {
      if (feature.RequiresDelegatedAvailabilityCheck() &&
          !delegated_availability_check_handler_.is_null()) {
        feature.SetDelegatedAvailabilityCheckHandler(
            delegated_availability_check_handler_);
      }
      return visitor(feature);
    };

    bool should_continue = false;
    switch (complex_feature_data_->feature_type) {
      case ComplexFeatureType::kSimple: {
        SimpleFeature feature(&data);
        should_continue = visit(feature);
        break;
      }
      case ComplexFeatureType::kManifest: {
        ManifestFeature feature(&data);
        should_continue = visit(feature);
        break;
      }
      case ComplexFeatureType::kPermission: {
        PermissionFeature feature(&data);
        should_continue = visit(feature);
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
    const ContextData& context_data) const {
  return FindFirstAvailability([&](const Feature& feature) {
    return feature.IsAvailableToContextImpl(extension, context, url, platform,
                                            context_id, check_developer_mode,
                                            context_data);
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
  // Constructor verifies that composed features are consistent, thus we can
  // return just the first feature's value.
  return complex_feature_data_->features.span().front().config.is_internal;
}

bool ComplexFeature::RequiresDelegatedAvailabilityCheck() const {
  return requires_delegated_availability_check_;
}

void ComplexFeature::SetDelegatedAvailabilityCheckHandler(
    DelegatedAvailabilityCheckHandler handler) {
  DCHECK(RequiresDelegatedAvailabilityCheck());
  DCHECK(!HasDelegatedAvailabilityCheckHandler());

  delegated_availability_check_handler_ = std::move(handler);
}

bool ComplexFeature::HasDelegatedAvailabilityCheckHandler() const {
  return !delegated_availability_check_handler_.is_null();
}

}  // namespace extensions
