// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_view_controller_delegate.h"

class BringAndroidTabsToIOSService;
class FaviconLoader;
@protocol TabListFromAndroidConsumer;
class UrlLoadingBrowserAgent;

// Mediator for the "Tab List From Android" table.
@interface TabListFromAndroidMediator
    : NSObject <TableViewFaviconDataSource,
                TabListFromAndroidViewControllerDelegate>

// The main consumer for this mediator.
@property(nonatomic, weak) id<TabListFromAndroidConsumer> consumer;

// Designated initializer for the mediator. `service` is used to load the user's
// tabs to bring to iOS from their Android device.
- (instancetype)
    initWithBringAndroidTabsService:(BringAndroidTabsToIOSService*)service
                          URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                      faviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_MEDIATOR_H_
