// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_ANDROID_SWITCHER_SCREEN_CONSUMER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_ANDROID_SWITCHER_SCREEN_CONSUMER_H_

#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_consumer.h"

// Feature shown in the Lens slot of the Android Switcher promo.
enum class AndroidSwitcherLensSlotFeature {
  // Google Lens, shown when it is available.
  kLens,
  // Safe Browsing, shown in place of Lens when Lens is unavailable.
  kSafeBrowsing,
};

// Consumer protocol for the Android Switcher Default Browser promo screen.
@protocol
    DefaultBrowserAndroidSwitcherScreenConsumer <DefaultBrowserScreenConsumer>

// Sets the feature shown in the Lens slot of the promo. Needs to be called
// before the view is loaded.
- (void)setLensSlotFeature:(AndroidSwitcherLensSlotFeature)lensSlotFeature;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_ANDROID_SWITCHER_SCREEN_CONSUMER_H_
