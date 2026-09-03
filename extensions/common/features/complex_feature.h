// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_

#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr_exclusion.h"
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

// A ComplexFeature is composed of one or many Features. A ComplexFeature
// is available if any Feature (i.e. permission rule) that composes it is
// available, but not if only some combination of Features is available.
class ComplexFeature : public Feature {
 public:
  explicit ComplexFeature(StaticFeatureData<ComplexFeatureData> data);

  ComplexFeature(const ComplexFeature&) = delete;
  ComplexFeature& operator=(const ComplexFeature&) = delete;

  ~ComplexFeature() override;

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
  explicit ComplexFeature(const ComplexFeatureData* data);

  // Feature:
  Availability IsAvailableToContextImpl(
      const Extension* extension,
      mojom::ContextType context,
      const GURL& url,
      Platform platform,
      int context_id,
      bool check_developer_mode,
      const ContextData& context_data) const override;

  bool IsInternal() const override;

  bool RequiresDelegatedAvailabilityCheck() const override;
  void SetDelegatedAvailabilityCheckHandler(
      DelegatedAvailabilityCheckHandler handler) override;
  bool HasDelegatedAvailabilityCheckHandler() const override;

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
  // Safe to exclude because StaticFeatureData requires static storage.
  RAW_PTR_EXCLUSION const ComplexFeatureData* complex_feature_data_ = nullptr;

  // If any of the Features comprising this class requires a delegated
  // availability check, then this flag is set to true.
  bool requires_delegated_availability_check_{false};
  // Descriptor children are temporary, so their handler is reapplied on each
  // visit.
  DelegatedAvailabilityCheckHandler delegated_availability_check_handler_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
