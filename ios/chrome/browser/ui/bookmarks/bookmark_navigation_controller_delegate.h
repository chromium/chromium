// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_NAVIGATION_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_NAVIGATION_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// BookmarkNavigationControllerDelegate serves as a delegate for
// TableViewNavigationController.
// It ensures that the way the navigation controller responds to adaptive
// presentation changes is determined by the view it presents.
@interface BookmarkNavigationControllerDelegate
    : NSObject <UINavigationControllerDelegate>

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_NAVIGATION_CONTROLLER_DELEGATE_H_
