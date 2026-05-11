// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_FEATURE_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_FEATURE_AVAILABILITY_H_

#import <Foundation/Foundation.h>

struct AccountInfo;
class ProfileIOS;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace gemini {

// List of gemini features for which availability is conditional to conditions
//  beyond feature flag enablement.
enum class Feature {
  kImageRemix,
};

// Returns whether the specified feature is available for the given account.
bool IsFeatureAvailable(Feature feature, const AccountInfo& account_info);

// Returns whether the feature is available for the given profile.
bool IsFeatureAvailable(Feature feature, ProfileIOS* profile);

// Returns whether the feature is available for the given identity manager.
bool IsFeatureAvailable(Feature feature,
                        signin::IdentityManager* identity_manager);

// Returns whether the account capabilities permit using Gemini in Chrome.
bool HasGeminiInChromeCapability(const AccountInfo& account_info);

// Returns whether the account capabilities permit standard model execution
// features.
bool HasModelExecutionCapability(const AccountInfo& account_info);

}  // namespace gemini

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_FEATURE_AVAILABILITY_H_
