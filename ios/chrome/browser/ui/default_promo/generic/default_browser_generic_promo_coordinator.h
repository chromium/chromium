// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/generic/default_browser_generic_promo_commands.h"

@interface DefaultBrowserGenericPromoCoordinator : ChromeCoordinator

// Handler for all actions of this coordinator.
@property(nonatomic, weak) id<DefaultBrowserGenericPromoCommands> handler;

// Whether or not to show the Remind Me Later button.
@property(nonatomic, assign) BOOL hasRemindMeLater;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_COORDINATOR_H_
