// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_NAVIGATION_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_NAVIGATION_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol TableViewModalPresenting;

// BookmarkNavigationControllerDelegate serves as a delegate for
// TableViewNavigationController. It uses |modalController| to update the modal
// presentation state when view controllers are pushed onto or popped off of the
// navigation stack.
@interface BookmarkNavigationControllerDelegate
    : NSObject <UINavigationControllerDelegate>

// An object which controls the modal presentation of the navigation controller.
@property(nonatomic, weak) id<TableViewModalPresenting> modalController;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_NAVIGATION_CONTROLLER_DELEGATE_H_
