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

// Returns the full name of the AI tier. Returns nil if the tier is non-positive
// or unknown.
NSString* GetAITierFullName(int ai_tier);

// Returns the disc from which the premium ring is created.
// The disc should be resized first, then a 3px-width circle is extracted from
// it.
UIImage* GetPremiumDiscImage();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_SIGNIN_SIGNIN_AI_LOGO_H_
