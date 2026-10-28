// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_CHROME_CONNECTION_OPTIONS_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_CHROME_CONNECTION_OPTIONS_H_

#import <UIKit/UIKit.h>

// Wrapper around `UISceneConnectionOptions` that exposes the connection options
// used by Chrome and allows tests to construct connection options directly.
@interface ChromeConnectionOptions : NSObject

// A set of `NSUserActivity` objects to be handled when the scene connects.
@property(nonatomic, readonly) NSSet<NSUserActivity*>* userActivities;

// A set of `UIOpenURLContext` objects to be opened when the scene connects.
@property(nonatomic, readonly) NSSet<UIOpenURLContext*>* URLContexts;

// A home screen quick action selected by the user to launch the scene.
@property(nonatomic, readonly) UIApplicationShortcutItem* shortcutItem;

// Initializes an instance from `UISceneConnectionOptions`.
- (instancetype)initWithConnectionOptions:(UISceneConnectionOptions*)options;

// Designated initializer.
- (instancetype)initWithUserActivities:(NSSet<NSUserActivity*>*)userActivities
                           URLContexts:(NSSet<UIOpenURLContext*>*)URLContexts
                          shortcutItem:(UIApplicationShortcutItem*)shortcutItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_CHROME_CONNECTION_OPTIONS_H_
