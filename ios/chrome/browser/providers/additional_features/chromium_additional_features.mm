// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/additional_features/additional_features_api.h"
#import "ios/public/provider/chrome/browser/additional_features/additional_features_controller.h"

namespace ios {
namespace provider {

namespace {

// Public implementation for AdditionalFeaturesController.
class ChromiumAdditionalFeaturesController final
    : public AdditionalFeaturesController {
 public:
  inline void RegisterFeatureList(base::FeatureList* featureList) final {}
  inline void FeatureListDidCompleteSetup() final {}
};

}  // namespace

std::unique_ptr<AdditionalFeaturesController>
CreateAdditionalFeaturesController() {
  return std::make_unique<ChromiumAdditionalFeaturesController>();
}

}  // namespace provider
}  // namespace ios
