// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"

// The leading inset for the separator of UITableView without leading icons.
extern const CGFloat kTableViewSeparatorInset;
// The leading inset for the separator of UITableView with leading icons. This
// is the default value for LegacyChromeTableViewController.
extern const CGFloat kTableViewSeparatorInsetWithIcon;

@class ChromeTableViewStyler;
@class TableViewItem;

@protocol TableViewIllustratedEmptyViewDelegate;

// Chrome-specific TableViewController.
// *********************************************************
// *********************************************************
// Don't add new use of this class. Please use new DiffableDataSource API
// instead.
// *********************************************************
// *********************************************************
@interface LegacyChromeTableViewController
    : UITableViewController <LegacyChromeTableViewConsumer>

// The model of this controller.
@property(nonatomic, readonly, strong)
    TableViewModel<TableViewItem*>* tableViewModel;

// The styler that controls how this table view and its cells are
// displayed. Styler changes should be made before viewDidLoad is called; any
// changes made afterwards are not guaranteed to take effect.
@property(nonatomic, readwrite, strong) ChromeTableViewStyler* styler;

// Top offset of the empty view which will be applied in addition to the safe
// area insets. Useful when a non-content cell (eg. sign-in promo) is shown at
// the top of the table view.
@property(nonatomic, readwrite, assign) CGFloat emptyViewTopOffset;

// Initializes the view controller, configured with `style`. The default
// ChromeTableViewStyler will be used.
- (instancetype)initWithStyle:(UITableViewStyle)style NS_DESIGNATED_INITIALIZER;
// Unavailable initializers.
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Initializes the collection view model. Must be called by subclasses if they
// override this method in order to get a clean tableViewModel.
- (void)loadModel NS_REQUIRES_SUPER;

// Adds and starts a loading indicator in the center of the
// LegacyChromeTableViewController, if one is not already present. This will
// remove any existing table view background views.
- (void)startLoadingIndicatorWithLoadingMessage:(NSString*)loadingMessage;

// Removes and stops the loading indicator, if one is present.
- (void)stopLoadingIndicatorWithCompletion:(ProceduralBlock)completion;

// Adds an empty table view in the center of the LegacyChromeTableViewController
// which displays `message` with `image` on top.  `message` will be rendered
// using default styling.  This will remove any existing table view background
// views.
- (void)addEmptyTableViewWithMessage:(NSString*)message image:(UIImage*)image;

// Adds an empty table view in the center of the LegacyChromeTableViewController
// which displays `attributedMessage` with `image` on top.  This will remove any
// existing table view background views.
- (void)addEmptyTableViewWithAttributedMessage:
            (NSAttributedString*)attributedMessage
                                         image:(UIImage*)image;

// Adds an empty table view in the center of the LegacyChromeTableViewController
// which displays an image, a title and a subtitle. This will remove any
// existing table view background views.
- (void)addEmptyTableViewWithImage:(UIImage*)image
                             title:(NSString*)title
                          subtitle:(NSString*)subtitle;

// Adds an empty table view in the center of the LegacyChromeTableViewController
// which displays an image, a title and an attributed string as a subtitle. This
// will remove any existing table view background views. Nullable `delegate`
// argument will get notified when subtile link taps occur.
- (void)addEmptyTableViewWithImage:(UIImage*)image
                             title:(NSString*)title
                attributedSubtitle:(NSAttributedString*)subtitle
                          delegate:(id<TableViewIllustratedEmptyViewDelegate>)
                                       delegate;

// Updates the accessibility label of the empty view to `newLabel`.
- (void)updateEmptyTableViewAccessibilityLabel:(NSString*)newLabel;

// Removes the empty table view, if one is present.
- (void)removeEmptyTableView;

// Performs batch table view updates described by `updates`, using `completion`
// as the completion block.
- (void)performBatchTableViewUpdates:(void (^)(void))updates
                          completion:(void (^)(BOOL finished))completion;

// Removes the items at `indexPaths` from the model only. The changes need to be
// also done on the table view to avoid being in an inconsistent state.
- (void)removeFromModelItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths;

// Methods for reconfiguring and reloading the table view are provided by
// LegacyChromeTableViewConsumer.

#pragma mark - UITableViewDelegate

// Prevents non-editable (i.e. returns NO in `tableView:canEditRowAtIndexPath:`)
// items from being selected in editing mode, otherwise they will not have radio
// buttons ahead but still be selectable, which may cause a crash when trying to
// delete items based on `self.tableView.indexPathsForSelectedRows`.
- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_LEGACY_CHROME_TABLE_VIEW_CONTROLLER_H_
