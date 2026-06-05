// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_IDENTITY_DISC_BUTTON_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_IDENTITY_DISC_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui/user_account_image_update_delegate.h"

@class NewTabPageColorPalette;

// A button that visually represents the identity disc on the NTP.
// It manages its own size constraints, image/title configuration,
// error badge, and accessibility label based on sign-in state.
@interface NTPIdentityDiscButton : UIButton <UserAccountImageUpdateDelegate>

// Sets up constraints relative to the safe area trailing anchor.
- (void)setupConstraintsWithTrailingAnchor:(NSLayoutXAxisAnchor*)trailingAnchor;

// Updates the error badge state.
- (void)updateADPBadgeWithErrorFound:(BOOL)hasAccountError
                                name:(NSString*)name
                               email:(NSString*)email;

// Updates the button's visual configuration (fonts, insets, colors).
- (void)updateConfigurationWithPalette:(NewTabPageColorPalette*)colorPalette;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NTP_IDENTITY_DISC_BUTTON_H_
