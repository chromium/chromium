// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_BROWSER_LAUNCHER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_BROWSER_LAUNCHER_H_

#import "ios/chrome/browser/main/browser_provider_interface.h"

// This protocol defines the startup method for the application.
@protocol BrowserLauncher<NSObject>

// Cached launchOptions from AppState's -didFinishLaunchingWithOptions.
@property(nonatomic, retain) NSDictionary* launchOptions;

// Browser view information created during startup.
@property(nonatomic, readonly) id<BrowserProviderInterface>
    browserProviderInterface;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_BROWSER_LAUNCHER_H_
