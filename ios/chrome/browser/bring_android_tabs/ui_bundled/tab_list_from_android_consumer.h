// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_CONSUMER_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_CONSUMER_H_

#import <Foundation/Foundation.h>

@class TabListFromAndroidTableViewItem;

// Consumer for the Tab List From Android view.
@protocol TabListFromAndroidConsumer

// Sets the `items` displayed by this consumer.
- (void)setTabListItems:(NSArray<TabListFromAndroidTableViewItem*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_CONSUMER_H_
