// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_H_

#import <Foundation/Foundation.h>

class GURL;

// Model object representing details about an item from the tab grid.
@interface GridItem : NSObject

// Create an item with `title`, and `url`.
- (instancetype)initWithTitle:(NSString*)title
                          url:(GURL)URL NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The title for the grid cell.
@property(nonatomic, copy) NSString* title;

// The URL of the tab represented by the Grid cell.
@property(nonatomic, assign) GURL URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_H_
