// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_ADDITIONAL_FEATURES_ADDITIONAL_FEATURES_CONTROLLER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_ADDITIONAL_FEATURES_ADDITIONAL_FEATURES_CONTROLLER_H_

namespace base {
class FeatureList;
}  // namespace base

// Controller object to manage some features not in base::FeatureList. It
// enables these features to be overridden by field trials.
class AdditionalFeaturesController {
 public:
  AdditionalFeaturesController();

  AdditionalFeaturesController(const AdditionalFeaturesController&) = delete;
  AdditionalFeaturesController& operator=(const AdditionalFeaturesController&) =
      delete;

  virtual ~AdditionalFeaturesController();

  // Register `featureList` to be able to query additional features. This must
  // be called before the `feature_list` is finalized.
  virtual void RegisterFeatureList(base::FeatureList* feature_list) = 0;

  // Notifies the controller that `base::FeatureList::GetInstance()` is
  // finalized. This hooks the global feature list instance with other features
  // that are not declared with BASE_DECLARE_FEATURE.
  virtual void FeatureListDidCompleteSetup() = 0;
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_ADDITIONAL_FEATURES_ADDITIONAL_FEATURES_CONTROLLER_H_
