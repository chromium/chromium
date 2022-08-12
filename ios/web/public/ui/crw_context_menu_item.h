// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_UI_CRW_CONTEXT_MENU_ITEM_H_
#define IOS_WEB_PUBLIC_UI_CRW_CONTEXT_MENU_ITEM_H_

#import "base/ios/block_types.h"

#import <UIKit/UIKit.h>

// Wraps information needed to show a custom context menu.
@interface CRWContextMenuItem : NSObject

// ID, unique to the set of items to be shown at one time. ID are turned into
// temporary objc selectors so must follow proper naming convention.
@property(readonly, assign) NSString* ID;

// Label of the item.
@property(readonly, assign) NSString* title;

// Callback to execute on user tap.
@property(readonly, assign) ProceduralBlock action;

// Static ctor helper.
+ (CRWContextMenuItem*)itemWithID:(NSString*)ID
                            title:(NSString*)title
                           action:(ProceduralBlock)action;
@end

#endif  // IOS_WEB_PUBLIC_UI_CRW_CONTEXT_MENU_ITEM_H_
