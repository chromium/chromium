// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item_type.h"

// Represents a menu item in the Composebox menu.
@interface ComposeboxMenuItem : NSObject <NSCopying>

// The composebox menu item title.
@property(nonatomic, copy, readonly) NSString* title;
// The composebox menu item image.
@property(nonatomic, strong, readonly) UIImage* image;
// The composebox menu item type.
@property(nonatomic, assign, readonly) ComposeboxMenuItemType type;
// Whether the composebox menu item is disabled.
@property(nonatomic, readonly, getter=isDisabled) BOOL disabled;
// The composebox menu item favicon.
@property(nonatomic, strong, readonly) UIImage* favicon;

- (BOOL)isEqual:(id)object;
- (NSUInteger)hash;

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type
                     disabled:(BOOL)disabled
                      favicon:(UIImage*)favicon;

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type
                     disabled:(BOOL)disabled;

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type;

// Returns YES if the item is an attachment type.
- (BOOL)isAttachmentType;

@end

// Returns the accessibility identifier for the given menu item type.
NSString* AccessibilityIdentifierForMenuItemType(ComposeboxMenuItemType type);

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ITEM_H_
