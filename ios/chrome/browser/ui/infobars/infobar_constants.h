// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONSTANTS_H_

#import <UIKit/UIKit.h>

#include "base/time/time.h"

extern const int kInfobarBackgroundColor;
// Top padding for the infobar when the omnibox is in the bottom toolbar.
extern const CGFloat kInfobarTopPaddingBottomOmnibox;

// a11y identifier so that automation can tap on either infobar button
extern NSString* const kConfirmInfobarButton1AccessibilityIdentifier;
extern NSString* const kConfirmInfobarButton2AccessibilityIdentifier;

// The duration that the InfobarCoordinator banner will be presented for.
constexpr base::TimeDelta kInfobarBannerDefaultPresentationDuration =
    base::Seconds(8);

// The duration that a high priority presentation InfobarCoordinator
// banner will be presented for.
constexpr base::TimeDelta kInfobarBannerLongPresentationDuration =
    base::Seconds(15);

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONSTANTS_H_
