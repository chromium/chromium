// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_CONSUMER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/share_kit/model/sharing_state.h"

@protocol FacePileProviding;

// Consumer to allow the tab group model to send information to the tab group
// UI.
@protocol TabGroupConsumer

// Sets the group title.
- (void)setGroupTitle:(NSString*)title;

// Sets the group color.
- (void)setGroupColor:(UIColor*)color;

// Sets whether the group can be shared or not.
- (void)setShareAvailable:(BOOL)shareAvailable;

// Sets the sharing state of a group.
- (void)setSharingState:(tab_groups::SharingState)state;

// Sets the face pile provider to display the share button or the face pile.
- (void)setFacePileProvider:(id<FacePileProviding>)facePileProvider;

// Sets the text to the activity summary cell.
- (void)setActivitySummaryCellText:(NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUP_CONSUMER_H_
