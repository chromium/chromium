// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_ITEM_H_

#import <Foundation/Foundation.h>

// Model object representing an item in the tab switchers.
@interface TabSwitcherItem : NSObject

// Create an item with `identifier`, which cannot be nil.
- (instancetype)initWithIdentifier:(NSString*)identifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) NSString* identifier;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) BOOL hidesTitle;
@property(nonatomic, assign) BOOL showsActivity;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_ITEM_H_
