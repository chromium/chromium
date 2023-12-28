// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "extensions/common/context_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace extensions {

// A ComplexFeature is composed of one or many Features. A ComplexFeature
// is available if any Feature (i.e. permission rule) that composes it is
// available, but not if only some combination of Features is available.
class ComplexFeature : public Feature {
 public:
  // Takes ownership of Feature*s contained in |features|.
  explicit ComplexFeature(std::vector<Feature*>* features);

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

  using FeatureList = std::vector<std::unique_ptr<Feature>>;
  FeatureList features_;

  // If any of the Features comprising this class requires a delegated
  // availability check, then this flag is set to true.
  bool requires_delegated_availability_check_{false};
  bool has_delegated_availability_check_handler_{false};
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_COMPLEX_FEATURE_H_
