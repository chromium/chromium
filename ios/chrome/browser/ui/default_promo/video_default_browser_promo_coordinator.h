// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_VIDEO_DEFAULT_BROWSER_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_VIDEO_DEFAULT_BROWSER_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_commands.h"

@interface VideoDefaultBrowserPromoCoordinator : ChromeCoordinator

// Handler for all actions of this coordinator.
@property(nonatomic, weak) id<DefaultBrowserPromoCommands> handler;

// Add halfscreen view
@property(nonatomic, assign) BOOL isHalfScreen;

// Whether or not to show the Remind Me Later button.
@property(nonatomic, assign) BOOL showRemindMeLater;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_VIDEO_DEFAULT_BROWSER_PROMO_COORDINATOR_H_
