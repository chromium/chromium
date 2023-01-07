// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_reading_list_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_reading_list_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Accessibility identifier of the Reading List Infobar Modal Add
// Button.
NSString* const kReadingListInfobarModalAddButtonAXId =
    @"kReadingListInfobarModalAddButtonAXId";
// Accessibility identifier of the Reading List Infobar Modal Never Ask
// Button.
NSString* const kReadingListInfobarModalNeverAskButtonAXId =
    @"kReadingListInfobarModalNeverAskButtonAXId";
}  // namespace

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeInformationalText = kItemTypeEnumZero,
  ItemTypeAddButton,
  ItemTypeNeverButton,
};

@interface InfobarReadingListTableViewController ()

// InfobarReadingListModalDelegate for this ViewController.
@property(nonatomic, weak) id<InfobarReadingListModalDelegate>
    infobarModalDelegate;
// YES if the current page has already been added.
@property(nonatomic, assign) BOOL currentPageAdded;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

@end

@implementation InfobarReadingListTableViewController

- (instancetype)initWithDelegate:
    (id<InfobarReadingListModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeAddToReadingList];
    _infobarModalDelegate = modalDelegate;
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  [self loadModel];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.infobarModalDelegate modalInfobarWasDismissed:self];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
  [super viewDidDisappear:animated];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.tableView.scrollEnabled =
      self.tableView.contentSize.height > self.view.frame.size.height;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextItem* timeThresholdContextInformationalItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeInformationalText];
  timeThresholdContextInformationalItem.textColor =
      [UIColor colorNamed:kTextSecondaryColor];
  timeThresholdContextInformationalItem.textFont =
      [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
  timeThresholdContextInformationalItem.text =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_MESSAGES_MODAL_DESCRIPTION);
  [model addItem:timeThresholdContextInformationalItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextButtonItem* addButtonItem =
      [self textButtonItemForType:ItemTypeAddButton
                       buttonText:l10n_util::GetNSString(
                                      IDS_IOS_READING_LIST_MESSAGES_MAIN_ACTION)
                  accessibilityId:kReadingListInfobarModalAddButtonAXId];
  addButtonItem.disableButtonIntrinsicWidth = YES;
  addButtonItem.enabled = !self.currentPageAdded;
  [model addItem:addButtonItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextButtonItem* neverAskItem = [self
      textButtonItemForType:ItemTypeNeverButton
                 buttonText:l10n_util::GetNSString(
                                IDS_IOS_READING_LIST_MESSAGES_MODAL_NEVER_ASK)
            accessibilityId:kReadingListInfobarModalNeverAskButtonAXId];
  neverAskItem.buttonTextColor = [UIColor colorNamed:kRedColor];
  neverAskItem.buttonBackgroundColor = [UIColor clearColor];
  [model addItem:neverAskItem toSectionWithIdentifier:SectionIdentifierContent];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeAddButton: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
      tableViewTextButtonCell.selectionStyle =
          UITableViewCellSelectionStyleNone;
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(addToReadingListButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeNeverButton: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
      tableViewTextButtonCell.selectionStyle =
          UITableViewCellSelectionStyleNone;
      [tableViewTextButtonCell.button
                 addTarget:self.infobarModalDelegate
                    action:@selector(neverAskToAddToReadingList)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeInformationalText:
      break;
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return 0;
}

#pragma mark - InfobarReadingListModalConsumer

- (void)setCurrentPageAdded:(BOOL)pageAdded {
  _currentPageAdded = pageAdded;
}

#pragma mark - Private Methods

- (void)addToReadingListButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  [self.infobarModalDelegate modalInfobarButtonWasAccepted:self];
}

- (void)dismissInfobarModal {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.infobarModalDelegate dismissInfobarModal:self];
}

- (TableViewTextButtonItem*)textButtonItemForType:(ItemType)itemType
                                       buttonText:(NSString*)buttonText
                                  accessibilityId:(NSString*)allyId {
  TableViewTextButtonItem* item =
      [[TableViewTextButtonItem alloc] initWithType:itemType];
  item.boldButtonText = NO;
  item.buttonText = buttonText;
  item.buttonAccessibilityIdentifier = allyId;
  return item;
}

@end
