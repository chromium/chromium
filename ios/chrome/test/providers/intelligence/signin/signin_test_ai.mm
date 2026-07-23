// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/intelligence/signin/signin_ai_logo.h"

namespace ios::provider {

NSString* GetAITierName(int ai_tier) {
  if (ai_tier <= 0) {
    return nil;
  }
  return [NSString stringWithFormat:@"%d", ai_tier];
}

UIImage* GetPremiumRingImage() {
  return [UIImage imageNamed:@"premium_ring"];
}

}  // namespace ios::provider
