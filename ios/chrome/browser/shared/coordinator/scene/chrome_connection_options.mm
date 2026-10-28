// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/chrome_connection_options.h"

@implementation ChromeConnectionOptions

- (instancetype)initWithConnectionOptions:(UISceneConnectionOptions*)options {
  return [self initWithUserActivities:options.userActivities
                          URLContexts:options.URLContexts
                         shortcutItem:options.shortcutItem];
}

- (instancetype)initWithUserActivities:(NSSet<NSUserActivity*>*)userActivities
                           URLContexts:(NSSet<UIOpenURLContext*>*)URLContexts
                          shortcutItem:
                              (UIApplicationShortcutItem*)shortcutItem {
  if ((self = [super init])) {
    _userActivities = [userActivities copy];
    _URLContexts = [URLContexts copy];
    _shortcutItem = shortcutItem;
  }
  return self;
}

@end
