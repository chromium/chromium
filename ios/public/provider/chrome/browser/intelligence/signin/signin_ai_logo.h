// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_SIGNIN_SIGNIN_AI_LOGO_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_SIGNIN_SIGNIN_AI_LOGO_H_

#import <UIKit/UIKit.h>

namespace ios::provider {

// Returns the name of the AI tier. Returns nil if the tier is non-positive or
// unknown.
NSString* GetAITierName(int ai_tier);

// Returns the premium ring image.
UIImage* GetPremiumRingImage();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_SIGNIN_SIGNIN_AI_LOGO_H_
