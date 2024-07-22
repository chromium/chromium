// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_consumer.h"

@protocol QuickDeleteBrowsingDataViewControllerDelegate;
@protocol QuickDeleteMutator;

@interface QuickDeleteBrowsingDataViewController
    : ChromeTableViewController <QuickDeleteConsumer>

// Local dispatcher for this `QuickDeleteBrowsingDataViewController`.
@property(nonatomic, weak) id<QuickDeleteBrowsingDataViewControllerDelegate>
    delegate;

// Mutator to apply all user changes on the view.
@property(nonatomic, weak) id<QuickDeleteMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_VIEW_CONTROLLER_H_
