// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_omnibox_state_provider.h"

#import "ios/chrome/browser/shared/public/features/features.h"

@implementation BrowserOmniboxStateProvider

#pragma mark - OmniboxStateProvider

- (BOOL)isOmniboxFocused {
  if (IsChromeNextIaEnabled()) {
    return [self.composeboxStateProvider isOmniboxFocused];
  }
  // First check if the composebox is focused. If it is, return true.
  // Otherwise, check if the location bar is focused.
  return [self.composeboxStateProvider isOmniboxFocused] ||
         [self.locationBarStateProvider isOmniboxFocused];
}

@end
