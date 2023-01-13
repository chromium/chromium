// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_ITEM_H_

#import <Foundation/Foundation.h>

class GURL;

// Model object representing details about a tab item.
@interface TabItem : NSObject

// Create an item with `title`, and `URL`.
- (instancetype)initWithTitle:(NSString*)title
                          URL:(GURL)URL NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The title for the tab cell.
@property(nonatomic, copy) NSString* title;

// The URL of the tab represented by the tab cell.
@property(nonatomic, assign) GURL URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_ITEM_H_
