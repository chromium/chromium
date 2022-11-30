// Copyright 2022 The Chromium Authors
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
@property(readonly, strong) NSString* ID;

// Label of the item.
@property(readonly, strong) NSString* title;

// Icon of the item, if required.
@property(readonly, strong) UIImage* image;

// Callback to execute on user tap.
@property(readonly, strong) ProceduralBlock action;

// Static ctor helpers.
+ (CRWContextMenuItem*)itemWithID:(NSString*)ID
                            title:(NSString*)title
                           action:(ProceduralBlock)action;

+ (CRWContextMenuItem*)itemWithID:(NSString*)ID
                            title:(NSString*)title
                            image:(UIImage*)image
                           action:(ProceduralBlock)action;
@end

#endif  // IOS_WEB_PUBLIC_UI_CRW_CONTEXT_MENU_ITEM_H_
