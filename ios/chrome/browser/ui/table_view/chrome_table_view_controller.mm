// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/material_components/chrome_app_bar_view_controller.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_empty_view.h"
#import "ios/chrome/browser/ui/table_view/table_view_loading_view.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const CGFloat kTableViewSeparatorInset = 16;
const CGFloat kTableViewSeparatorInsetWithIcon = 56;

@interface ChromeTableViewController ()
// The loading displayed by [self startLoadingIndicatorWithLoadingMessage:].
@property(nonatomic, strong) TableViewLoadingView* loadingView;
// The view displayed by [self addEmptyTableViewWithMessage:].
@property(nonatomic, strong) TableViewEmptyView* emptyView;
@end

@implementation ChromeTableViewController
@synthesize appBarViewController = _appBarViewController;
@synthesize emptyView = _emptyView;
@synthesize loadingView = _loadingView;
@synthesize styler = _styler;
@synthesize tableViewModel = _tableViewModel;

- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle {
  if ((self = [super initWithStyle:style])) {
    _styler = [[ChromeTableViewStyler alloc] init];

    if (appBarStyle == ChromeTableViewControllerStyleWithAppBar) {
      _appBarViewController = [[ChromeAppBarViewController alloc] init];
    }
  }
  return self;
}

- (instancetype)init {
  return [self initWithTableViewStyle:UITableViewStylePlain
                          appBarStyle:ChromeTableViewControllerStyleNoAppBar];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.tableView setBackgroundColor:self.styler.tableViewBackgroundColor];
  [self.tableView setSeparatorColor:self.styler.cellSeparatorColor];
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewSeparatorInsetWithIcon, 0,
                                         0)];

  // Configure the app bar if needed.
  if (_appBarViewController) {
    ConfigureAppBarViewControllerWithCardStyle(self.appBarViewController);
    self.appBarViewController.headerView.trackingScrollView = self.tableView;
    // Add the AppBar's views after all other views have been registered.
    [self addChildViewController:_appBarViewController];
    CGRect frame = self.appBarViewController.view.frame;
    frame.origin.x = 0;
    frame.size.width =
        self.appBarViewController.parentViewController.view.bounds.size.width;
    self.appBarViewController.view.frame = frame;
    [self.view addSubview:self.appBarViewController.view];
    [self.appBarViewController didMoveToParentViewController:self];
  }
}

#pragma mark - UITableViewDelegate

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.editing && ![self tableView:tableView
                          canEditRowAtIndexPath:indexPath]) {
    return nil;
  }
  return indexPath;
}

#pragma mark - Accessors

- (void)setStyler:(ChromeTableViewStyler*)styler {
  DCHECK(![self isViewLoaded]);
  _styler = styler;
}

- (void)setEmptyView:(TableViewEmptyView*)emptyView {
  if (_emptyView == emptyView)
    return;
  _emptyView = emptyView;
  _emptyView.scrollViewContentInsets = self.view.safeAreaInsets;
  self.tableView.backgroundView = _emptyView;
  // Since this would replace any loadingView, set it to nil.
  self.loadingView = nil;
}

#pragma mark - Public

- (void)loadModel {
  _tableViewModel = [[TableViewModel alloc] init];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  // The safe area insets aren't propagated to the inner scroll view. Manually
  // set the content insets.
  self.emptyView.scrollViewContentInsets = self.view.safeAreaInsets;
}

- (void)startLoadingIndicatorWithLoadingMessage:(NSString*)loadingMessage {
  if (!self.loadingView) {
    self.loadingView =
        [[TableViewLoadingView alloc] initWithFrame:self.view.bounds
                                     loadingMessage:loadingMessage];
    self.tableView.backgroundView = self.loadingView;
    [self.loadingView startLoadingIndicator];
    // Since this would replace any emptyView, set it to nil.
    self.emptyView = nil;
  }
}

- (void)stopLoadingIndicatorWithCompletion:(ProceduralBlock)completion {
  if (self.loadingView) {
    [self.loadingView stopLoadingIndicatorWithCompletion:^{
      if (completion)
        completion();
      [self.loadingView removeFromSuperview];
      // Check that the tableView.backgroundView hasn't been modified
      // before its removed.
      DCHECK(self.tableView.backgroundView == self.loadingView);
      self.tableView.backgroundView = nil;
      self.loadingView = nil;
    }];
  }
}

- (void)addEmptyTableViewWithMessage:(NSString*)message image:(UIImage*)image {
  self.emptyView = [[TableViewEmptyView alloc] initWithFrame:self.view.bounds
                                                     message:message
                                                       image:image];
  self.emptyView.tintColor = [UIColor colorNamed:kPlaceholderImageTintColor];
}

- (void)addEmptyTableViewWithAttributedMessage:
            (NSAttributedString*)attributedMessage
                                         image:(UIImage*)image {
  self.emptyView = [[TableViewEmptyView alloc] initWithFrame:self.view.bounds
                                           attributedMessage:attributedMessage
                                                       image:image];
  self.emptyView.tintColor = [UIColor colorNamed:kPlaceholderImageTintColor];
}

- (void)updateEmptyTableViewMessageAccessibilityLabel:(NSString*)newLabel {
  self.emptyView.messageAccessibilityLabel = newLabel;
}

- (void)removeEmptyTableView {
  if (self.emptyView) {
    // Check that the tableView.backgroundView hasn't been modified
    // before its removed.
    DCHECK(self.tableView.backgroundView == self.emptyView);
    self.tableView.backgroundView = nil;
    self.emptyView = nil;
  }
}

- (void)performBatchTableViewUpdates:(void (^)(void))updates
                          completion:(void (^)(BOOL finished))completion {
  [self.tableView performBatchUpdates:updates completion:completion];
}

- (void)removeFromModelItemAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  // Sort and enumerate in reverse order to delete the items from the collection
  // view model.
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  for (NSIndexPath* indexPath in [sortedIndexPaths reverseObjectEnumerator]) {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSection:indexPath.section];
    NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
    NSUInteger index =
        [self.tableViewModel indexInItemTypeForIndexPath:indexPath];
    [self.tableViewModel removeItemWithType:itemType
                  fromSectionWithIdentifier:sectionIdentifier
                                    atIndex:index];
  }
}

#pragma mark - ChromeTableViewConsumer

- (void)reconfigureCellsForItems:(NSArray*)items {
  for (TableViewItem* item in items) {
    NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];

    // |cell| may be nil if the row is not currently on screen.
    if (cell) {
      TableViewCell* tableViewCell =
          base::mac::ObjCCastStrict<TableViewCell>(cell);
      [item configureCell:tableViewCell withStyler:self.styler];
    }
  }
}

- (void)reloadCellsForItems:(NSArray*)items
           withRowAnimation:(UITableViewRowAnimation)rowAnimation {
  if (![items count])
    return;
  NSMutableArray* indexPathsToReload = [[NSMutableArray alloc] init];
  for (TableViewItem* item in items) {
    NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
    [indexPathsToReload addObject:indexPath];
  }
  if ([indexPathsToReload count])
    [self.tableView reloadRowsAtIndexPaths:indexPathsToReload
                          withRowAnimation:rowAnimation];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  Class cellClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(cellClass);
  [self.tableView registerClass:cellClass
         forCellReuseIdentifier:reuseIdentifier];
  UITableViewCell* cell =
      [self.tableView dequeueReusableCellWithIdentifier:reuseIdentifier
                                           forIndexPath:indexPath];
  TableViewCell* tableViewCell = base::mac::ObjCCastStrict<TableViewCell>(cell);
  [item configureCell:tableViewCell withStyler:self.styler];

  return cell;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [self.tableViewModel numberOfItemsInSection:section];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return [self.tableViewModel numberOfSections];
}

#pragma mark - Presentation Controller integration

- (BOOL)shouldBeDismissedOnTouchOutside {
  return YES;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  TableViewHeaderFooterItem* item =
      [self.tableViewModel headerForSection:section];
  if (!item)
    return [[UIView alloc] initWithFrame:CGRectZero];
  Class headerFooterClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(headerFooterClass);
  [self.tableView registerClass:headerFooterClass
      forHeaderFooterViewReuseIdentifier:reuseIdentifier];
  UITableViewHeaderFooterView* view = [self.tableView
      dequeueReusableHeaderFooterViewWithIdentifier:reuseIdentifier];
  [item configureHeaderFooterView:view withStyler:self.styler];
  return view;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  TableViewHeaderFooterItem* item =
      [self.tableViewModel footerForSection:section];
  if (!item)
    return [[UIView alloc] initWithFrame:CGRectZero];
  Class headerFooterClass = [item cellClass];
  NSString* reuseIdentifier = NSStringFromClass(headerFooterClass);
  [self.tableView registerClass:headerFooterClass
      forHeaderFooterViewReuseIdentifier:reuseIdentifier];
  UITableViewHeaderFooterView* view = [self.tableView
      dequeueReusableHeaderFooterViewWithIdentifier:reuseIdentifier];
  [item configureHeaderFooterView:view withStyler:self.styler];
  return view;
}

#pragma mark - MDCAppBarViewController support

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidScroll];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDecelerating];
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDraggingWillDecelerate:decelerate];
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView
        trackingScrollViewWillEndDraggingWithVelocity:velocity
                                  targetContentOffset:targetContentOffset];
  }
}

@end
