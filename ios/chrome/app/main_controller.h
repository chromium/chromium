// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_CONTROLLER_H_
#define IOS_CHROME_APP_MAIN_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"

@class AppState;
@protocol BrowserProviderInterface;
@class MetricsMediator;

// The main controller of the application, owned by the MainWindow nib. Also
// serves as the delegate for the app. Owns all the various top-level
// UI controllers.
//
// By design, it has no public API of its own. Anything interacting with
// MainController should be doing so through a specific protocol.
@interface MainController : NSObject <StartupInformation, AppStateObserver>

// Contains information about the application state, for example whether the
// safe mode is activated.
@property(nonatomic, weak) AppState* appState;

// This metrics mediator is used to check and update the metrics accordingly to
// to the user preferences.
@property(nonatomic, weak) MetricsMediator* metricsMediator;

// The BrowserProviderInterface for the foreground scene, or for any background
// connected scene if there's no foreground scene. If there are none of these,
// this is `nil`.
// TODO(crbug.com/40266840) Remove this public property.
@property(nonatomic, readonly) id<BrowserProviderInterface>
    browserProviderInterfaceDoNotUse;

@end

#endif  // IOS_CHROME_APP_MAIN_CONTROLLER_H_
