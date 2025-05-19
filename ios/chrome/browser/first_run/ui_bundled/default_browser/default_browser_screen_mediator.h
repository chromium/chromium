// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol DefaultBrowserScreenConsumer;

// Mediator for presenting the Default Browser promo with the updated First Run
// Experience.
@interface DefaultBrowserScreenMediator : NSObject

// Main consumer for this mediator.
@property(nonatomic, weak) id<DefaultBrowserScreenConsumer> consumer;
// Contains the user choice for UMA reporting. This value is set to the default
// value when the coordinator is initialized.
@property(nonatomic, assign) BOOL UMAReportingUserChoice;
// Whether the user tapped on the TOS link.
@property(nonatomic, assign) BOOL TOSLinkWasTapped;
// Whether the user tapped on the UMA link.
@property(nonatomic, assign) BOOL UMALinkWasTapped;

// Disconnects this mediator.
- (void)disconnect;

// Called when the coordinator is finished.
- (void)finishPresenting;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_
