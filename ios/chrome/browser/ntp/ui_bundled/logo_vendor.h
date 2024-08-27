// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_LOGO_VENDOR_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_LOGO_VENDOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/ui_bundled/logo_animation_controller.h"

namespace web {
class WebState;
}  // namespace web

// Observer to listen for when the doodle is shown and hidden.
@protocol DoodleObserver <NSObject>

// Notifies observer that the display state of the doodle has changed.
- (void)doodleDisplayStateChanged:(BOOL)showingDoodle;

@end

// Defines a controller whose view contains a doodle or search engine logo.
@protocol LogoVendor <LogoAnimationControllerOwnerOwner, NSObject>

// View that shows a doodle or a search engine logo.
@property(nonatomic, readonly, retain) UIView* view;

// Whether or not the logo should be shown.  Defaults to YES.
@property(nonatomic, assign, getter=isShowingLogo) BOOL showingLogo;

// Whether the logo should be multicolor or monochrome.
@property(nonatomic, assign) BOOL usesMonochromeLogo;

// Whether or not the doodle is being shown. Defaults to NO.
- (BOOL)isShowingDoodle;

// Listening to DoodleObserver.
@property(nonatomic, weak) id<DoodleObserver> doodleObserver;

// Checks for a new doodle.  Calling this method frequently will result in a
// query being issued at most once per hour.
- (void)fetchDoodle;

// Updates the vendor's WebState.
- (void)setWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_LOGO_VENDOR_H_
