// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_APP_INTERFACE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// ReadingListAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface ReadingListAppInterface : NSObject

// Removes all entries in the ReadingListModel.
+ (NSError*)clearEntries [[nodiscard]];

// Adds an entry in the ReadingListModel.
+ (NSError*)addEntryWithURL:(NSURL*)url
                      title:(NSString*)title
                       read:(BOOL)read [[nodiscard]];

// The number of read entries in the ReadingListModel.
+ (NSInteger)readEntriesCount;

// The number of unread entries in the ReadingListModel.
+ (NSInteger)unreadEntriesCount;

// Simulate that the current connection is WiFI.
+ (void)forceConnectionToWifi;

// Sends a notification that connection is WiFi.
+ (void)notifyWifiConnection;

// Reset the connection type.
+ (void)resetConnectionType;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_APP_INTERFACE_H_
