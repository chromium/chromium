// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_ACTIVITY_LABEL_VIEW_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_ACTIVITY_LABEL_VIEW_H_

#import <UIKit/UIKit.h>

// A view for the activity label on grid cell or group grid cell.
@interface ActivityLabelView : UIView

// Sets the text to the UILabel in the activity label.
- (void)setLabelText:(NSString*)text;

// Sets the icon to the UIView in the activity label. The view will be hidden if
// `iconView` is nil.
- (void)setUserIcon:(UIView*)iconView;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_ACTIVITY_LABEL_VIEW_H_
