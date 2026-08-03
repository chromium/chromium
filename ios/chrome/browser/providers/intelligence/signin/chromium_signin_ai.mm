// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/public/provider/chrome/browser/intelligence/signin/signin_ai_logo.h"

namespace ios::provider {

// The values returned here are for testing purposes only. They are non-branded
// values and images that can be used for testing. They will never appear in
// Chromium (because the user cannot sign in) nor in Chrome (where another
// provider implementation overrides them).

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

UIImage* GetPremiumDiscImage() {
  return [UIImage imageNamed:@"premium_disc"];
}

}  // namespace ios::provider
