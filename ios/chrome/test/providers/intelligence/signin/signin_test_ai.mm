// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/intelligence/signin/signin_ai_logo.h"

// The content of this file is used for debugging purpose only.
// Neither those strings nor images are expected to ever be seen by the user,
// as the AI tiers can only be non-0 if the user is signed-in, which is not
// possible on chromium.

namespace ios::provider {

NSString* GetAITierName(int ai_tier) {
  if (ai_tier <= 0) {
    return nil;
  }
  return [NSString stringWithFormat:@"%d", ai_tier];
}

NSString* GetAITierFullName(int ai_tier) {
  NSString* name = GetAITierName(ai_tier);
  if (!name) {
    return nil;
  }
  return [NSString stringWithFormat:@"AI %@", name];
}

UIImage* GetPremiumRingImage() {
  return [UIImage imageNamed:@"premium_disk"];
}

}  // namespace ios::provider
