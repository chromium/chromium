// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

#include <stddef.h>

#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"

namespace extensions {

ComplexFeature::ComplexFeature(StaticFeatureData<ComplexFeatureData> data)
    : ComplexFeature(data.get()) {}

ComplexFeature::ComplexFeature(const ComplexFeatureData* data)
    : Feature(&data->feature) {
  CHECK_GT(data->features.span().size(), 1u);
  features_.reserve(data->features.span().size());
  for (const auto& feature_data : data->features.span()) {
    std::unique_ptr<Feature> feature;
    switch (data->feature_type) {
      case ComplexFeatureType::kSimple:
        feature = base::WrapUnique(new SimpleFeature(&feature_data));
        break;
      case ComplexFeatureType::kManifest:
        feature = base::WrapUnique(new ManifestFeature(&feature_data));
        break;
      case ComplexFeatureType::kPermission:
        feature = base::WrapUnique(new PermissionFeature(&feature_data));
        break;
    }
    CHECK(feature);
    requires_delegated_availability_check_ |=
        feature->RequiresDelegatedAvailabilityCheck();
    features_.push_back(std::move(feature));
  }

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  const bool first_is_internal = features_.front()->IsInternal();
  for (const auto& feature : features_) {
    DCHECK_EQ(first_is_internal, feature->IsInternal())
        << "Complex feature must have consistent values of "
           "internal across all sub features.";
    DCHECK_EQ(no_parent(), feature->no_parent())
        << "Complex feature must have consistent values of "
           "no_parent across all sub features.";
  }
#endif
}

ComplexFeature::~ComplexFeature() = default;

bool ComplexFeature::VisitFeatures(
    base::FunctionRef<bool(Feature&)> visitor) const {
  for (const auto& feature : features_) {
    if (!visitor(*feature)) {
      return false;
    }
  }
  return true;
}

Feature::Availability ComplexFeature::IsAvailableToManifest(
    const HashedExtensionId& hashed_id,
    Manifest::Type type,
    mojom::ManifestLocation location,
    int manifest_version,
    Platform platform,
    int context_id) const {
  Availability first_availability = features_.front()->IsAvailableToManifest(
      hashed_id, type, location, manifest_version, platform, context_id);
  if (first_availability.is_available()) {
    return first_availability;
  }

  for (size_t i = 1; i < features_.size(); ++i) {
    Availability availability = features_[i]->IsAvailableToManifest(
        hashed_id, type, location, manifest_version, platform, context_id);
    if (availability.is_available()) {
      return availability;
    }
  }
  return first_availability;
}

Feature::Availability ComplexFeature::IsAvailableToContextImpl(
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    Platform platform,
    int context_id,
    bool check_developer_mode,
    const ContextData& context_data) const {
  Availability first_availability = features_.front()->IsAvailableToContextImpl(
      extension, context, url, platform, context_id, check_developer_mode,
      context_data);
  if (first_availability.is_available()) {
    return first_availability;
  }

  for (size_t i = 1; i < features_.size(); ++i) {
    Availability availability = features_[i]->IsAvailableToContextImpl(
        extension, context, url, platform, context_id, check_developer_mode,
        context_data);
    if (availability.is_available()) {
      return availability;
    }
  }
  return first_availability;
}

Feature::Availability ComplexFeature::IsAvailableToEnvironment(
    int context_id) const {
  Availability first_availability =
      features_.front()->IsAvailableToEnvironment(context_id);
  if (first_availability.is_available()) {
    return first_availability;
  }

  for (size_t i = 1; i < features_.size(); ++i) {
    Availability availability =
        features_[i]->IsAvailableToEnvironment(context_id);
    if (availability.is_available()) {
      return availability;
    }
  }
  return first_availability;
}

bool ComplexFeature::IsIdInBlocklist(const HashedExtensionId& hashed_id) const {
  for (const auto& feature : features_) {
    if (feature->IsIdInBlocklist(hashed_id)) {
      return true;
    }
  }
  return false;
}

bool ComplexFeature::IsIdInAllowlist(const HashedExtensionId& hashed_id) const {
  for (const auto& feature : features_) {
    if (feature->IsIdInAllowlist(hashed_id)) {
      return true;
    }
  }
  return false;
}

bool ComplexFeature::IsInternal() const {
  // Constructor verifies that composed features are consistent, thus we can
  // return just the first feature's value.
  return features_.front()->IsInternal();
}

bool ComplexFeature::RequiresDelegatedAvailabilityCheck() const {
  return requires_delegated_availability_check_;
}

void ComplexFeature::SetDelegatedAvailabilityCheckHandler(
    DelegatedAvailabilityCheckHandler handler) {
  DCHECK(RequiresDelegatedAvailabilityCheck());
  DCHECK(!HasDelegatedAvailabilityCheckHandler());

  for (const auto& feature : features_) {
    if (feature->RequiresDelegatedAvailabilityCheck()) {
      feature->SetDelegatedAvailabilityCheckHandler(handler);
    }
  }
  has_delegated_availability_check_handler_ = true;
}

bool ComplexFeature::HasDelegatedAvailabilityCheckHandler() const {
  return has_delegated_availability_check_handler_;
}

}  // namespace extensions
