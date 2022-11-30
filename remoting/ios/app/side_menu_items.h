// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_SIDE_MENU_ITEMS_H_
#define REMOTING_IOS_APP_SIDE_MENU_ITEMS_H_

#import <UIKit/UIKit.h>

typedef void (^SideMenuItemAction)(void);

// Represents an item on the side menu.
@interface SideMenuItem : NSObject

- (instancetype)initWithTitle:(NSString*)title
                         icon:(UIImage*)icon
                       action:(SideMenuItemAction)action;

@property(nonatomic, readonly) NSString* title;
@property(nonatomic, readonly) UIImage* icon;
@property(nonatomic, readonly) SideMenuItemAction action;

@end

// Class that provides the list of SideMenuItems to be shown on the side menu.
@interface SideMenuItemsProvider : NSObject

// Each item is located by sideMenuItems[indexPath.section][indexPath.item]
@property(nonatomic, readonly, class)
    NSArray<NSArray<SideMenuItem*>*>* sideMenuItems;

@end

#endif  // REMOTING_IOS_APP_SIDE_MENU_ITEMS_H_
