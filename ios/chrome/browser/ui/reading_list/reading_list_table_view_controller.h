// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_accessibility_delegate.h"

@protocol ReadingListDataSource;
@protocol ReadingListListViewControllerAudience;
@protocol ReadingListListViewControllerDelegate;
@protocol ReadingListMenuProvider;
@class SigninPromoViewConfigurator;
@protocol SigninPromoViewDelegate;

class Browser;

// View controller that displays reading list items in a table view.
@interface ReadingListTableViewController
    : LegacyChromeTableViewController <ReadingListListItemAccessibilityDelegate,
                                       UIAdaptivePresentationControllerDelegate>

// The delegate.
@property(nonatomic, weak) id<ReadingListListViewControllerDelegate> delegate;
// The audience that is interested in whether the table has any items.
@property(nonatomic, weak) id<ReadingListListViewControllerAudience> audience;
// The table's data source.
@property(nonatomic, weak) id<ReadingListDataSource> dataSource;
// The browser.
@property(nonatomic, assign) Browser* browser;
// Provider of menu configurations for the readingList component.
@property(nonatomic, weak) id<ReadingListMenuProvider> menuProvider;

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Prepares this view controller to be dismissed.
- (void)willBeDismissed;

// Reloads all the data.
- (void)reloadData;

// Controls the visibility state of the sign-in promo.
- (void)promoStateChanged:(BOOL)promoEnabled
        promoConfigurator:(SigninPromoViewConfigurator*)promoConfigurator
            promoDelegate:(id<SigninPromoViewDelegate>)promoDelegate
                promoText:(NSString*)promoText;

// Updates the sign-in promo view after identity updates.
- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)promoConfigurator
                             identityChanged:(BOOL)identityChanged;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_TABLE_VIEW_CONTROLLER_H_
