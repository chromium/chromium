// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_CELL_H_

#import <UIKit/UIKit.h>

// UICollectionViewCell that contains a Tab title with a leading imageView
// and a close tab button.
@interface TabStripCell : UICollectionViewCell

@property(nonatomic, strong) UILabel* titleLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_STRIP_TAB_STRIP_CELL_H_
