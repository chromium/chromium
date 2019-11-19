// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_BROWSER_LAUNCHER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_BROWSER_LAUNCHER_H_

#import "ios/chrome/browser/ui/main/browser_interface_provider.h"

// Possible stages of the browser initialization. These states will be reached
// in sequence, each stage is a requiremant for the following one.
enum BrowserInitializationStageType {
  // This state is before any initialization in MainController.
  INITIALIZATION_STAGE_NONE = 0,
  // Initialization state after |didFinishLaunchingWithOptions|.
  INITIALIZATION_STAGE_BASIC,
  // Initialization state needed by background handlers.
  INITIALIZATION_STAGE_BACKGROUND,
  // Full initialization of the browser.
  INITIALIZATION_STAGE_FOREGROUND,
  BROWSER_INITIALIZATION_STAGE_TYPE_COUNT,
};

// This protocol defines the startup method for the application.
@protocol BrowserLauncher<NSObject>

// Cached launchOptions from AppState's -didFinishLaunchingWithOptions.
@property(nonatomic, retain) NSDictionary* launchOptions;

// Highest initialization stage reached by the browser.
@property(nonatomic, readonly)
    BrowserInitializationStageType browserInitializationStage;

// Browser view information created during startup.
@property(nonatomic, readonly) id<BrowserInterfaceProvider> interfaceProvider;

// Initializes the application up to |stage|. This is safe to call multiple
// times for the same value of |stage|; the actions for each stage will only
// be run once during the lifetime of the app.
- (void)startUpBrowserToStage:(BrowserInitializationStageType)stage;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_BROWSER_LAUNCHER_H_
