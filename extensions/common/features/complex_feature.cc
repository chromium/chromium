// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/complex_feature.h"

#include "extensions/common/mojom/context_type.mojom.h"
namespace extensions {

ComplexFeature::ComplexFeature(std::vector<Feature*>* features) {
  DCHECK_GT(features->size(), 1UL);
  for (Feature* f : *features) {
    features_.push_back(std::unique_ptr<Feature>(f));
    requires_delegated_availability_check_ |=
        f->RequiresDelegatedAvailabilityCheck();
  }
  features->clear();
  no_parent_ = features_[0]->no_parent();

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  // Verify IsInternal and no_parent are consistent across all features.
  bool first_is_internal = features_[0]->IsInternal();
  for (FeatureList::const_iterator it = features_.begin() + 1;
       it != features_.end();
       ++it) {
    DCHECK(first_is_internal == (*it)->IsInternal())
        << "Complex feature must have consistent values of "
           "internal across all sub features.";
    DCHECK(no_parent_ == (*it)->no_parent())
        << "Complex feature must have consistent values of "
           "no_parent across all sub features.";
  }
#endif
}

ComplexFeature::~ComplexFeature() = default;

Feature::Availability ComplexFeature::IsAvailableToManifest(
    const HashedExtensionId& hashed_id,
    Manifest::Type type,
    mojom::ManifestLocation location,
    int manifest_version,
    Platform platform,
    int context_id) const {
  Feature::Availability first_availability =
      features_[0]->IsAvailableToManifest(
          hashed_id, type, location, manifest_version, platform, context_id);
  if (first_availability.is_available())
    return first_availability;

  for (auto it = features_.cbegin() + 1; it != features_.cend(); ++it) {
    Availability availability = (*it)->IsAvailableToManifest(
        hashed_id, type, location, manifest_version, platform, context_id);
    if (availability.is_available())
      return availability;
  }
  // If none of the SimpleFeatures are available, we return the availability
  // info of the first SimpleFeature that was not available.
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
  Feature::Availability first_availability =
      features_[0]->IsAvailableToContextImpl(extension, context, url, platform,
                                             context_id, check_developer_mode,
                                             context_data);
  if (first_availability.is_available())
    return first_availability;

  for (auto it = features_.cbegin() + 1; it != features_.cend(); ++it) {
    Availability availability = (*it)->IsAvailableToContextImpl(
        extension, context, url, platform, context_id, check_developer_mode,
        context_data);
    if (availability.is_available())
      return availability;
  }
  // If none of the SimpleFeatures are available, we return the availability
  // info of the first SimpleFeature that was not available.
  return first_availability;
}

Feature::Availability ComplexFeature::IsAvailableToEnvironment(
    int context_id) const {
  Feature::Availability first_availability =
      features_[0]->IsAvailableToEnvironment(context_id);
  if (first_availability.is_available())
    return first_availability;

  for (auto iter = features_.cbegin() + 1; iter != features_.cend(); ++iter) {
    Availability availability = (*iter)->IsAvailableToEnvironment(context_id);
    if (availability.is_available())
      return availability;
  }
  // If none of the SimpleFeatures are available, we return the availability
  // info of the first SimpleFeature that was not available.
  return first_availability;
}

bool ComplexFeature::IsIdInBlocklist(const HashedExtensionId& hashed_id) const {
  for (auto it = features_.cbegin(); it != features_.cend(); ++it) {
    if ((*it)->IsIdInBlocklist(hashed_id))
      return true;
  }
  return false;
}

bool ComplexFeature::IsIdInAllowlist(const HashedExtensionId& hashed_id) const {
  for (auto it = features_.cbegin(); it != features_.cend(); ++it) {
    if ((*it)->IsIdInAllowlist(hashed_id))
      return true;
  }
  return false;
}

bool ComplexFeature::IsInternal() const {
  // Constructor verifies that composed features are consistent, thus we can
  // return just the first feature's value.
  return features_[0]->IsInternal();
}

bool ComplexFeature::RequiresDelegatedAvailabilityCheck() const {
  return requires_delegated_availability_check_;
}

void ComplexFeature::SetDelegatedAvailabilityCheckHandler(
    DelegatedAvailabilityCheckHandler handler) {
  DCHECK(RequiresDelegatedAvailabilityCheck());
  DCHECK(!HasDelegatedAvailabilityCheckHandler());

  // Set the given handler on all of the sub-feature that need a delegated
  // availability check handler and set
  // |has_delegated_availability_check_handler_| to true.
  for (auto& feature : features_) {
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
