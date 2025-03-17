// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/toolbar/ui_bundled/banner_promo_view.h"

@class DefaultBrowserBannerPromoAppAgent;
@protocol PrimaryToolbarConsumer;
@protocol SettingsCommands;

// Mediator providing state information to PrimaryToolbarViewController.
@interface PrimaryToolbarMediator : NSObject <BannerPromoViewDelegate>

// Consumer to alert of UI changes.
@property(nonatomic, weak) id<PrimaryToolbarConsumer> consumer;

// Handler for any settings commands.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

- (instancetype)initWithDefaultBrowserBannerPromoAppAgent:
    (DefaultBrowserBannerPromoAppAgent*)defaultBrowserBannerAppAgent
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Stops any observation and disconnects any C++ references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_MEDIATOR_H_
