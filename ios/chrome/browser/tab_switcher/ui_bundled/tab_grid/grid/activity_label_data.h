// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_ACTIVITY_LABEL_DATA_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_ACTIVITY_LABEL_DATA_H_

#import <UIKit/UIKit.h>

@protocol ShareKitAvatarPrimitive;

// Holds properties to configure an activity label.
@interface ActivityLabelData : NSObject

// The string of the activity label.
@property(nonatomic, copy) NSString* labelString;

// The object to provide an avatar image. The image is resolved asynchronously
// by `-resolve:`.
@property(nonatomic, strong) id<ShareKitAvatarPrimitive> avatarPrimitive;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_ACTIVITY_LABEL_DATA_H_
