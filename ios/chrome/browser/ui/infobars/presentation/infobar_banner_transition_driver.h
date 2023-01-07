// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_TRANSITION_DRIVER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_TRANSITION_DRIVER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_interaction_delegate.h"

@protocol InfobarBannerPositioner;

// The transition delegate used to present an InfobarBanner.
@interface InfobarBannerTransitionDriver
    : NSObject <UIViewControllerTransitioningDelegate,
                InfobarBannerInteractionDelegate>

// Delegate used to position the InfobarBanner.
@property(nonatomic, weak) id<InfobarBannerPositioner> bannerPositioner;

// Completes the banner presentation if taking place. This will stop the banner
// animation and move it to the presenting ViewController hierarchy.
// This method should be called if trying to dismiss the banner before its
// presentation has finished.
- (void)completePresentationTransitionIfRunning;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_TRANSITION_DRIVER_H_
