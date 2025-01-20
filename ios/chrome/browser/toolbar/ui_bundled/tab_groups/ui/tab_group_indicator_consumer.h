// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_CONSUMER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_CONSUMER_H_

#import <Foundation/Foundation.h>

// TabGroupIndicator Consumer interface.
@protocol TabGroupIndicatorConsumer <NSObject>

// Sets the `groupTitle` and the `groupColor` to be displayed in the view.
- (void)setTabGroupTitle:(NSString*)groupTitle groupColor:(UIColor*)groupColor;

// Sets whether the group can be shared or not.
- (void)setShareAvailable:(BOOL)shareAvailable;

// Sets the group shared state. YES when this group is shared with other users.
- (void)setShared:(BOOL)shared;

// Sets the face pile view controller to display the share button or the face
// pile.
- (void)setFacePileViewController:(UIViewController*)facePileViewController;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_CONSUMER_H_
