// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_TEST_UTIL_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_TEST_UTIL_H_

#include "extensions/common/features/feature.h"

namespace extensions {

class FeatureTestPeer {
 public:
  class ScopedDelegatedAvailabilityCheckHandlers {
   public:
    ScopedDelegatedAvailabilityCheckHandlers(
        const Feature& feature,
        Feature::DelegatedAvailabilityCheckHandler handler);

    explicit ScopedDelegatedAvailabilityCheckHandlers(
        Feature::FeatureDelegatedAvailabilityCheckMap handlers);

    ScopedDelegatedAvailabilityCheckHandlers(
        const ScopedDelegatedAvailabilityCheckHandlers&) = delete;
    ScopedDelegatedAvailabilityCheckHandlers& operator=(
        const ScopedDelegatedAvailabilityCheckHandlers&) = delete;

    ~ScopedDelegatedAvailabilityCheckHandlers();

   private:
    Feature::FeatureDelegatedAvailabilityCheckMap previous_handlers_;
  };

  static Feature::DelegatedAvailabilityCheckHandler
  GetDelegatedAvailabilityCheckHandler(const Feature& feature) {
    return feature.delegated_availability_check_handler();
  }
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_TEST_UTIL_H_
