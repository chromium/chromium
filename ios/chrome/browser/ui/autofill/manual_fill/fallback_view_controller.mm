// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_view_controller.h"

#include "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  ItemsSectionIdentifier = kSectionIdentifierEnumZero,
  ActionsSectionIdentifier,
};

namespace {

// This is the width used for `self.preferredContentSize`.
constexpr CGFloat PopoverPreferredWidth = 320;

// This is the maximum height used for `self.preferredContentSize`.
constexpr CGFloat PopoverMaxHeight = 360;

// This is the height used for `self.preferredContentSize` when showing the
// loading indicator on iPad.
constexpr CGFloat PopoverLoadingHeight = 185.5;

// If the loading indicator was shown, it will be on screen for at least this
// amount of seconds.
constexpr CGFloat kMinimumLoadingTime = 0.5;

// Height of the section header.
constexpr CGFloat kSectionHeaderHeight = 6;

// Height of the section footer.
constexpr CGFloat kSectionFooterHeight = 8;

}  // namespace

@interface FallbackViewController ()

// The date when the loading indicator started or [NSDate distantPast] if it
// hasn't been shown.
@property(nonatomic, strong) NSDate* loadingIndicatorStartingDate;

// Data Items to be shown when the loading indicator disappears.
@property(nonatomic, strong) NSArray<TableViewItem*>* queuedDataItems;

// Action Items to be shown when the loading indicator disappears.
@property(nonatomic, strong) NSArray<TableViewItem*>* queuedActionItems;

@end

@implementation FallbackViewController

- (instancetype)init {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _loadingIndicatorStartingDate = [NSDate distantPast];
  }
  return self;
}

- (void)viewDidLoad {
  // Super's `viewDidLoad` uses `styler.tableViewBackgroundColor` so it needs to
  // be set before.
  self.styler.tableViewBackgroundColor = [UIColor colorNamed:kBackgroundColor];

  [super viewDidLoad];

  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.sectionHeaderHeight = kSectionHeaderHeight;
  self.tableView.sectionFooterHeight = kSectionFooterHeight;
  self.tableView.estimatedRowHeight = 1;
  self.tableView.separatorInset = UIEdgeInsetsMake(0, 0, 0, 0);
  self.tableView.allowsSelection = NO;
  self.definesPresentationContext = YES;
  if (!self.tableViewModel) {
    if (self.popoverPresentationController) {
      self.preferredContentSize = CGSizeMake(
          PopoverPreferredWidth, AlignValueToPixel(PopoverLoadingHeight));
    }
    [self startLoadingIndicatorWithLoadingMessage:@""];
    self.loadingIndicatorStartingDate = [NSDate date];
  }
}

- (void)presentDataItems:(NSArray<TableViewItem*>*)items {
  if (![self shouldPresentItems]) {
    if (self.queuedDataItems) {
      self.queuedDataItems = items;
      return;
    }
    self.queuedDataItems = items;
    NSTimeInterval remainingTime =
        kMinimumLoadingTime - [self timeSinceLoadingIndicatorStarted];
    __weak __typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 (int64_t)(remainingTime * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [weakSelf presentQueuedDataItems];
                   });
    return;
  }
  self.queuedDataItems = items;
  [self presentQueuedDataItems];
}

- (void)presentActionItems:(NSArray<TableViewItem*>*)actions {
  if (![self shouldPresentItems]) {
    if (self.queuedActionItems) {
      self.queuedActionItems = actions;
      return;
    }
    self.queuedActionItems = actions;
    NSTimeInterval remainingTime =
        kMinimumLoadingTime - [self timeSinceLoadingIndicatorStarted];
    __weak __typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 (int64_t)(remainingTime * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [weakSelf presentQueuedActionItems];
                   });
    return;
  }
  self.queuedActionItems = actions;
  [self presentQueuedActionItems];
}

#pragma mark - Private

// Presents the data items currently in queue.
- (void)presentQueuedDataItems {
  DCHECK(self.queuedDataItems);
  [self createModelIfNeeded];
  BOOL sectionExist = [self.tableViewModel
      hasSectionForSectionIdentifier:ItemsSectionIdentifier];
  // If there are no passed items, remove section if exist.
  if (!self.queuedDataItems.count && sectionExist) {
    [self.tableViewModel removeSectionWithIdentifier:ItemsSectionIdentifier];
  } else if (self.queuedDataItems.count && !sectionExist) {
    [self.tableViewModel insertSectionWithIdentifier:ItemsSectionIdentifier
                                             atIndex:0];
  }
  [self presentFallbackItems:self.queuedDataItems
                   inSection:ItemsSectionIdentifier];
  self.queuedDataItems = nil;
}

// Presents the action items currently in queue.
- (void)presentQueuedActionItems {
  DCHECK(self.queuedActionItems);
  [self createModelIfNeeded];
  BOOL sectionExist = [self.tableViewModel
      hasSectionForSectionIdentifier:ActionsSectionIdentifier];
  // If there are no passed items, remove section if exist.
  if (!self.queuedActionItems.count && sectionExist) {
    [self.tableViewModel removeSectionWithIdentifier:ActionsSectionIdentifier];
  } else if (self.queuedActionItems.count && !sectionExist) {
    [self.tableViewModel addSectionWithIdentifier:ActionsSectionIdentifier];
  }
  [self presentFallbackItems:self.queuedActionItems
                   inSection:ActionsSectionIdentifier];
  self.queuedActionItems = nil;
}

// Seconds since the loading indicator started. This is >> kMinimumLoadingTime
// if the loading indicator wasn't shown.
- (NSTimeInterval)timeSinceLoadingIndicatorStarted {
  return
      [[NSDate date] timeIntervalSinceDate:self.loadingIndicatorStartingDate];
}

// Indicates if the view is ready for data to be presented.
- (BOOL)shouldPresentItems {
  return [self timeSinceLoadingIndicatorStarted] >= kMinimumLoadingTime;
}

- (void)createModelIfNeeded {
  if (!self.tableViewModel) {
    [self loadModel];
    [self stopLoadingIndicatorWithCompletion:nil];
  }
}

// Presents `items` in the respective section. Handles creating or deleting the
// section accordingly.
- (void)presentFallbackItems:(NSArray<TableViewItem*>*)items
                   inSection:(SectionIdentifier)sectionIdentifier {
  // If there are no passed items, remove section if exist.
  if (items.count) {
    [self.tableViewModel
        deleteAllItemsFromSectionWithIdentifier:sectionIdentifier];
    for (TableViewItem* item in items) {
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:sectionIdentifier];
    }
  }
  [self.tableView reloadData];
  if (self.popoverPresentationController) {
    // Update the preffered content size on iPad so the popover shows the right
    // size.
    [self.tableView layoutIfNeeded];
    CGSize systemLayoutSize = self.tableView.contentSize;
    CGFloat preferredHeight = MIN(systemLayoutSize.height, PopoverMaxHeight);
    self.preferredContentSize =
        CGSizeMake(PopoverPreferredWidth, preferredHeight);
  }
}

@end
