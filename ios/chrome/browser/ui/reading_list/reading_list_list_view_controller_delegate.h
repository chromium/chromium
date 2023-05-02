// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class ListItem;
@protocol ReadingListListItem;

// Delegate protocol for actions performed by the list view implementations,
// managing the visibility of the toolbar, dismissing the Reading List View and
// opening elements.
@protocol ReadingListListViewControllerDelegate<NSObject>

// Dismisses the Reading List View.
- (void)dismissReadingListListViewController:
    (UIViewController*)readingListCollectionViewController;

// Opens `item.entryURL`.
- (void)readingListListViewController:(UIViewController*)viewController
                             openItem:(id<ReadingListListItem>)item;

// Opens the entry corresponding to the `item` in a new tab, `incognito` or not.
- (void)readingListListViewController:(UIViewController*)viewController
                     openItemInNewTab:(id<ReadingListListItem>)item
                            incognito:(BOOL)incognito;

// Opens the offline version of the entry corresponding to the `item` in a new
// tab, if available.
- (void)readingListListViewController:(UIViewController*)viewController
              openItemOfflineInNewTab:(id<ReadingListListItem>)item;

// Notifies the delegate that the reading list has been loaded.
- (void)didLoadContent;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_CONTROLLER_DELEGATE_H_
