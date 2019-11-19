// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_collection_view_controller.h"

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_consumer.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@class ClearBrowsingDataManager;

// CollectionView for clearing browsing data (including history,
// cookies, caches, passwords, and autofill).
@interface ClearBrowsingDataCollectionViewController
    : SettingsRootCollectionViewController <ClearBrowsingDataConsumer>

// "Default" convenience initializer that Users should in general make use of.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState;

// Designated initializer to allow dependency injection (in tests).
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                             manager:(ClearBrowsingDataManager*)manager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_COLLECTION_VIEW_CONTROLLER_H_
