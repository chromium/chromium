// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MENU_NOTIFICATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MENU_NOTIFICATION_DELEGATE_H_

#import <Foundation/Foundation.h>

// Protocol to implement in order to be delegate for reading list changes
// impacting the menu.
@protocol ReadingListMenuNotificationDelegate<NSObject>

// Called when the reading list menu unread count has changed.
- (void)unreadCountChanged:(NSInteger)unreadCount;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_MENU_NOTIFICATION_DELEGATE_H_
