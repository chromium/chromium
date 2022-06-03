// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_DELEGATE_H_

#import <UIKit/UIKit.h>

// Delegate to handle InfobarBanner actions.
@protocol InfobarBannerDelegate

// Called when the InfobarBanner button was pressed.
- (void)bannerInfobarButtonWasPressed:(UIButton*)sender;

// Asks the delegate to dismiss the banner UI.  |userInitiated| is YES if
// directly triggered by a user action (e.g. swiping up the banner), and NO for
// all other cases, even if the banner is dismissed indirectly by a user action
// (e.g. accepting the banner, presenting settings).
- (void)dismissInfobarBannerForUserInteraction:(BOOL)userInitiated;

// Asks the delegate to present the InfobarModal for this InfobarBanner.
- (void)presentInfobarModalFromBanner;

// Informs the delegate that the banner has been dismissed.
- (void)infobarBannerWasDismissed;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_DELEGATE_H_
