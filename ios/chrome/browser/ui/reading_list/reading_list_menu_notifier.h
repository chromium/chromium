// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MENU_NOTIFIER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MENU_NOTIFIER_H_

#import <Foundation/Foundation.h>

class ReadingListModel;
@protocol ReadingListMenuNotificationDelegate;

// Notifies its delegate of changes in the reading list that have an impact on
// the menu. Can also be queried for current values of the model.
@interface ReadingListMenuNotifier : NSObject

// Delegate for handling of changes in the reading list model.
@property(nonatomic, weak) id<ReadingListMenuNotificationDelegate> delegate;

- (instancetype)initWithReadingList:(ReadingListModel*)readingListModel;

// The number of unread items in the reading list.
- (NSInteger)readingListUnreadCount;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MENU_NOTIFIER_H_
