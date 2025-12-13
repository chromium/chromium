// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_SAVE_CARD_SAVE_CARD_INFOBAR_BANNER_OVERLAY_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_SAVE_CARD_SAVE_CARD_INFOBAR_BANNER_OVERLAY_MEDIATOR_TESTING_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"

// Category for exposing properties and methods for testing.
@interface SaveCardInfobarBannerOverlayMediator (Testing)

// Block for posting accessibility notifications. Used for testing.
@property(nonatomic, copy) void (^accessibilityNotificationPoster)
    (UIAccessibilityNotifications, id);

// Sets an override for the VoiceOver status for testing purposes.
- (void)setOverrideVoiceOverForTesting:(bool)value;

// Clears any VoiceOver status override.
- (void)clearOverrideVoiceOverForTesting;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_SAVE_CARD_SAVE_CARD_INFOBAR_BANNER_OVERLAY_MEDIATOR_TESTING_H_
