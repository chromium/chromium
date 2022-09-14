// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_HEADER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_HEADER_H_

#import <UIKit/UIKit.h>

// A view that can be used as a section header in a grid. Contains a title, and
// a value labels.
@interface GridHeader : UICollectionReusableView
// Text added on the leading side of the header.
@property(nonatomic, copy) NSString* title;
// Text added on the trailing side of the header. if it's nil or empty string -
// Only the `title` will be rendered.
@property(nonatomic, copy) NSString* value;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_HEADER_H_
