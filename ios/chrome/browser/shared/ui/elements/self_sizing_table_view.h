// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_SELF_SIZING_TABLE_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_SELF_SIZING_TABLE_VIEW_H_

#import <UIKit/UIKit.h>

// A table view that calculates its intrinsic content height and exposes it
// through intrinsicContentSize property. The intrinsic size is invalidated
// automatically whenever the table view content is changed.
@interface SelfSizingTableView : UITableView
@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_SELF_SIZING_TABLE_VIEW_H_
