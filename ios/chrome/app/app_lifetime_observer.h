// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APP_LIFETIME_OBSERVER_H_
#define IOS_CHROME_APP_APP_LIFETIME_OBSERVER_H_

#import <UIKit/UIKit.h>

@class MemoryWarningHelper;

// Protocol representing a way to observer Application lifetime events.
//
// There is no way to register to observe those events, this protocol exists
// to clean the public API of the MainController and ProfileController which
// are the two classes that implements this protocol.
@protocol AppLifetimeObserver

// Called when the application will resign being the active application.
- (void)applicationWillResignActive:(UIApplication*)application;

// Called when the application is terminating. Gives a last opportunity to
// save any pending state and to perform any necessary cleanup.
- (void)applicationWillTerminate:(UIApplication*)application;

// Called when the application is going into the background.
- (void)applicationDidEnterBackground:(UIApplication*)application
                         memoryHelper:(MemoryWarningHelper*)memoryHelper;

// Called when the application is going into the foreground.
- (void)applicationWillEnterForeground:(UIApplication*)application
                          memoryHelper:(MemoryWarningHelper*)memoryHelper;

// Called when the application discards a set of scene sessions. These sessions
// can no longer be accessed and all their associated data should be destroyed.
- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions;

@end

#endif  // IOS_CHROME_APP_APP_LIFETIME_OBSERVER_H_
