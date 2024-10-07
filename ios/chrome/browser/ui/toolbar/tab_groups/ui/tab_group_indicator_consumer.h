// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_CONSUMER_H_

#import <Foundation/Foundation.h>

// TabGroupIndicator Consumer interface.
@protocol TabGroupIndicatorConsumer <NSObject>

// Sets the `groupTitle` and the `groupColor` to be displayed in the view.
- (void)setTabGroupTitle:(NSString*)groupTitle groupColor:(UIColor*)groupColor;

// Sets whether the group can be shared or not.
- (void)setShareAvailable:(BOOL)shareAvailable;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_CONSUMER_H_
