// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/autofill/save_card_message_with_links.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCardLastDigits = kItemTypeEnumZero,
  ItemTypeCardHolderName,
  ItemTypeCardExpireMonth,
  ItemTypeCardExpireYear,
  ItemTypeCardLegalMessage,
  ItemTypeCardSave,
};

@interface InfobarSaveCardTableViewController () <TableViewTextLinkCellDelegate,
                                                  UITextFieldDelegate>

// InfobarSaveCardModalDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarSaveCardModalDelegate>
    saveCardModalDelegate;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;
// Starting index in the SectionIdentifierContent for the legalMessages. Used to
// query the corresponding SaveCardMessageWithLinks from legalMessages when
// configuring the cell.
@property(nonatomic, assign) int legalMessagesStartingIndex;

@end

@implementation InfobarSaveCardTableViewController

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveCardModalDelegate>)modalDelegate {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _saveCardModalDelegate = modalDelegate;
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveCard];
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal:)];
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
  [self.saveCardModalDelegate modalInfobarWasDismissed:self];
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

  TableViewTextEditItem* cardLastDigitsItem = [self
      textEditItemWithType:ItemTypeCardExpireYear
             textFieldName:l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARD_NUMBER)
            textFieldValue:self.cardNumber
          textFieldEnabled:NO];
  cardLastDigitsItem.identifyingIcon = self.cardIssuerIcon;
  [model addItem:cardLastDigitsItem
      toSectionWithIdentifier:SectionIdentifierContent];

  // TODO(crbug.com/1014652): Change textFieldEnabled to YES once editing its
  // supported.
  TableViewTextEditItem* cardholderNameItem =
      [self textEditItemWithType:ItemTypeCardExpireYear
                   textFieldName:l10n_util::GetNSString(
                                     IDS_IOS_AUTOFILL_CARDHOLDER_NAME)
                  textFieldValue:self.cardholderName
                textFieldEnabled:NO];
  [model addItem:cardholderNameItem
      toSectionWithIdentifier:SectionIdentifierContent];

  // TODO(crbug.com/1014652): Change textFieldEnabled to YES once editing its
  // supported.
  TableViewTextEditItem* expireMonthItem = [self
      textEditItemWithType:ItemTypeCardExpireYear
             textFieldName:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_MONTH)
            textFieldValue:self.expirationMonth
          textFieldEnabled:NO];
  [model addItem:expireMonthItem
      toSectionWithIdentifier:SectionIdentifierContent];

  // TODO(crbug.com/1014652): Change textFieldEnabled to YES once editing its
  // supported.
  TableViewTextEditItem* expireYearItem = [self
      textEditItemWithType:ItemTypeCardExpireYear
             textFieldName:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_YEAR)
            textFieldValue:self.expirationYear
          textFieldEnabled:NO];
  [model addItem:expireYearItem
      toSectionWithIdentifier:SectionIdentifierContent];

  // Set legalMessagesStartingIndex right before adding any
  // SaveCardMessageWithLinks TableViewTextLinkItems to the model.
  self.legalMessagesStartingIndex =
      [model numberOfItemsInSection:
                 [model sectionForSectionIdentifier:SectionIdentifierContent]];
  for (SaveCardMessageWithLinks* message in self.legalMessages) {
    TableViewTextLinkItem* legalMessageItem =
        [[TableViewTextLinkItem alloc] initWithType:ItemTypeCardLegalMessage];
    legalMessageItem.text = message.messageText;
    [model addItem:legalMessageItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }

  TableViewTextButtonItem* saveCardButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeCardSave];
  saveCardButtonItem.textAlignment = NSTextAlignmentNatural;
  saveCardButtonItem.buttonText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
  saveCardButtonItem.enabled = self.currentCardSaved;
  saveCardButtonItem.disableButtonIntrinsicWidth = YES;
  [model addItem:saveCardButtonItem
      toSectionWithIdentifier:SectionIdentifierContent];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeCardLastDigits: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeCardHolderName: {
      TableViewTextEditCell* editCell =
          base::mac::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(nameEditDidBegin)
                   forControlEvents:UIControlEventEditingDidBegin];
      [editCell.textField addTarget:self
                             action:@selector(updateSaveCardButtonState)
                   forControlEvents:UIControlEventEditingChanged];
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypeCardExpireMonth: {
      TableViewTextEditCell* editCell =
          base::mac::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(monthEditDidBegin)
                   forControlEvents:UIControlEventEditingDidBegin];
      [editCell.textField addTarget:self
                             action:@selector(updateSaveCardButtonState)
                   forControlEvents:UIControlEventEditingChanged];
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypeCardExpireYear: {
      TableViewTextEditCell* editCell =
          base::mac::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(yearEditDidBegin)
                   forControlEvents:UIControlEventEditingDidBegin];
      [editCell.textField addTarget:self
                             action:@selector(updateSaveCardButtonState)
                   forControlEvents:UIControlEventEditingChanged];
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypeCardLegalMessage: {
      NSUInteger legalMessageIndex =
          indexPath.row - self.legalMessagesStartingIndex;
      DCHECK(legalMessageIndex >= 0);
      DCHECK(legalMessageIndex < self.legalMessages.count);
      TableViewTextLinkCell* linkCell =
          base::mac::ObjCCast<TableViewTextLinkCell>(cell);
      SaveCardMessageWithLinks* message = self.legalMessages[legalMessageIndex];
      [message.linkRanges enumerateObjectsUsingBlock:^(
                              NSValue* rangeValue, NSUInteger i, BOOL* stop) {
        [linkCell setLinkURL:message.linkURLs[i]
                    forRange:rangeValue.rangeValue];
      }];
      linkCell.delegate = self;
      linkCell.separatorInset =
          UIEdgeInsetsMake(0, self.tableView.bounds.size.width, 0, 0);
      break;
    }
    case ItemTypeCardSave: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(saveCardButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return 0;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - TableViewTextLinkCellDelegate

- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(const GURL&)URL {
  [self.saveCardModalDelegate dismissModalAndOpenURL:URL];
}

#pragma mark - Private Methods

- (void)updateSaveCardButtonState {
  // TODO(crbug.com/1014652): Implement

  // TODO(crbug.com/1014652):Ideally the InfobarDelegate should update the
  // button text. Once we have a consumer protocol we should be able to create a
  // delegate that asks the InfobarDelegate for the correct text.
}

- (void)saveCardButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  // TODO(crbug.com/1014652): Use current item values once editing is supported.
  [self.saveCardModalDelegate saveCardWithCardholderName:self.cardholderName
                                         expirationMonth:self.expirationMonth
                                          expirationYear:self.expirationYear];
}

- (void)nameEditDidBegin {
  // TODO(crbug.com/1014652): Implement, should only be needed to record
  // SaveCard specific editing metrics.
}

- (void)monthEditDidBegin {
  // TODO(crbug.com/1014652): Implement, should only be needed to record
  // SaveCard specific editing metrics.
}

- (void)yearEditDidBegin {
  // TODO(crbug.com/1014652): Implement, should only be needed to record
  // SaveCard specific editing metrics.
}

- (void)dismissInfobarModal:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.saveCardModalDelegate dismissInfobarModal:sender
                                         animated:YES
                                       completion:nil];
}

#pragma mark - Helpers

- (TableViewTextEditItem*)textEditItemWithType:(ItemType)type
                                 textFieldName:(NSString*)name
                                textFieldValue:(NSString*)value
                              textFieldEnabled:(BOOL)enabled {
  TableViewTextEditItem* textEditItem =
      [[TableViewTextEditItem alloc] initWithType:type];
  textEditItem.textFieldName = name;
  textEditItem.textFieldValue = value;
  textEditItem.textFieldEnabled = enabled;
  textEditItem.hideIcon = !enabled;
  textEditItem.returnKeyType = UIReturnKeyDone;

  return textEditItem;
}

@end
