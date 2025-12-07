// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_H_

#import <UIKit/UIKit.h>

// Holds a snapshot and a favicon to configure a tab item.
@interface TabSnapshotAndFavicon : NSObject

@property(nonatomic, strong) UIImage* snapshot;
@property(nonatomic, strong) UIImage* favicon;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SNAPSHOT_AND_FAVICON_H_
