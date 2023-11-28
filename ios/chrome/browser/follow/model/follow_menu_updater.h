// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_MENU_UPDATER_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_MENU_UPDATER_H_

@class WebPageURLs;

// Protocol defining a updater for follow menu item.
@protocol FollowMenuUpdater

// Updates the follow menu item with follow `webPage`, `followed`,
// `domainName` and `enabled`.
- (void)updateFollowMenuItemWithWebPage:(WebPageURLs*)webPageURLs
                               followed:(BOOL)followed
                             domainName:(NSString*)domainName
                                enabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_MENU_UPDATER_H_
