// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COMPONENT_FACTORY_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COMPONENT_FACTORY_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_component_factory_protocol.h"

// These values are persisted to IOS.NTP.LensButtonNewBadgeShown histograms.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSNTPNewBadgeShownResult {
  kShown = 0,
  kNotShownLimitReached = 1,
  kNotShownButtonPressed = 2,
  kMaxValue = kNotShownButtonPressed,
};

extern const char kNTPLensButtonNewBadgeShownHistogram[];

// A factory which generates various NTP components for the
// NewTabPageCoordinator.
@interface NewTabPageComponentFactory
    : NSObject <NewTabPageComponentFactoryProtocol>
@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_COMPONENT_FACTORY_H_
