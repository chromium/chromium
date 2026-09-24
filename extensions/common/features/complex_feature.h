// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_

#include <stddef.h>

#include <type_traits>

#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "extensions/common/context_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace extensions {

enum class ComplexFeatureType {
  kSimple,
  kManifest,
  kPermission,
};

struct ComplexFeatureData {
  FeatureData feature;
  StaticSpan<SimpleFeatureData> features;
  ComplexFeatureType feature_type;
};

// descriptor() recovers the descriptor from the FeatureData pointer Feature
// holds, which requires the two to be pointer-interconvertible.
static_assert(std::is_standard_layout_v<ComplexFeatureData>,
              "ComplexFeatureData must be standard-layout so that a pointer to "
              "its first member can be cast back to the descriptor");
static_assert(offsetof(ComplexFeatureData, feature) == 0u,
              "FeatureData must be the first member of ComplexFeatureData");

// Named after the invariant they enforce so that the name appears in the
// diagnostic. See ValidateFeatureDescriptor in feature.h.
consteval void ComplexFeatureMustCombineMoreThanOneFeature() {
  FeatureDescriptorInvariantViolated();
}
consteval void ComplexFeatureChildrenMustAgreeOnIsInternal() {
  FeatureDescriptorInvariantViolated();
}
consteval void ComplexFeatureChildrenMustAgreeOnNoParent() {
  FeatureDescriptorInvariantViolated();
}

consteval void ValidateFeatureDescriptor(const ComplexFeatureData& data) {
  const base::span<const SimpleFeatureData> features = data.features.span();
  if (features.size() <= 1u) {
    ComplexFeatureMustCombineMoreThanOneFeature();
  }
  for (const SimpleFeatureData& child : features) {
    if (child.config.is_internal != features.front().config.is_internal) {
      ComplexFeatureChildrenMustAgreeOnIsInternal();
    }
    if (child.feature.no_parent != data.feature.no_parent) {
      ComplexFeatureChildrenMustAgreeOnNoParent();
    }
  }
}

// A ComplexFeature is composed of one or many Features. A ComplexFeature
// is available if any Feature (i.e. permission rule) that composes it is
// available, but not if only some combination of Features is available.
class ComplexFeature : public Feature {
 public:
  constexpr explicit ComplexFeature(StaticFeatureData<ComplexFeatureData> data)
      : ComplexFeature(data.get()) {}

  ComplexFeature(const ComplexFeature&) = delete;
  ComplexFeature& operator=(const ComplexFeature&) = delete;

  ~ComplexFeature() override = default;

  // extensions::Feature:
  Availability IsAvailableToManifest(const HashedExtensionId& hashed_id,
                                     Manifest::Type type,
                                     mojom::ManifestLocation location,
                                     int manifest_version,
                                     Platform platform,
                                     int context_id) const override;
  Availability IsAvailableToEnvironment(int context_id) const override;
  bool IsIdInBlocklist(const HashedExtensionId& hashed_id) const override;
  bool IsIdInAllowlist(const HashedExtensionId& hashed_id) const override;

 protected:
  constexpr explicit ComplexFeature(const ComplexFeatureData* data)
      : Feature(data ? &data->feature : nullptr) {}

  // Feature:
  Availability IsAvailableToContextImpl(
      const Extension* extension,
      mojom::ContextType context,
      const GURL& url,
      Platform platform,
      int context_id,
      bool check_developer_mode,
      const ContextData& context_data,
      DelegatedAvailabilityCheckHandler delegated_handler) const override;

  bool IsInternal() const override;

  bool RequiresDelegatedAvailabilityCheck() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FeaturesGenerationTest, FeaturesTest);
  FRIEND_TEST_ALL_PREFIXES(ComplexFeatureTest,
                           RequiresDelegatedAvailabilityCheck);

  // Returns false if `visitor` stops iteration by returning false. Descriptor
  // children are temporary and must not be retained by the visitor.
  bool VisitFeatures(base::FunctionRef<bool(const Feature&)> visitor) const;
  // Returns the first available child result, or the first child's failure if
  // no child is available.
  Availability FindFirstAvailability(
      base::FunctionRef<Availability(const Feature&)> get_availability) const;

  // The descriptor this feature was constructed from. Recovered from the
  // FeatureData pointer the base class holds, since FeatureData is
  // ComplexFeatureData's first member; see the static_asserts above.
  const ComplexFeatureData& descriptor() const;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
