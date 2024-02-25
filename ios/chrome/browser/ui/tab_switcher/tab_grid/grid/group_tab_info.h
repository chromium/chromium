// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_TAB_INFO_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_TAB_INFO_H_

#import <UIKit/UIKit.h>

// This object holds the necessary elements (snapshot and favicon) to configure
// a `GroupGridCell`.
@interface GroupTabInfo : NSObject

@property(nonatomic, strong) UIImage* snapshot;
@property(nonatomic, strong) UIImage* favicon;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_TAB_INFO_H_
