// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_ADDITIONAL_FEATURES_ADDITIONAL_FEATURES_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_ADDITIONAL_FEATURES_ADDITIONAL_FEATURES_API_H_

#import <memory>

#import "ios/public/provider/chrome/browser/additional_features/additional_features_controller.h"

namespace ios {
namespace provider {

// Returns a controller that manages features not declared using
// BASE_DECLARE_FEATURE. Since components embedded by Chromium and works through
// provider APIs might have no access to `//base`, they can use this API to
// declare new features dynamically.
std::unique_ptr<AdditionalFeaturesController>
CreateAdditionalFeaturesController();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_ADDITIONAL_FEATURES_ADDITIONAL_FEATURES_API_H_
