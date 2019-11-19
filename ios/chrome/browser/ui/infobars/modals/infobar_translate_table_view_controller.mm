// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSourceLanguage = kItemTypeEnumZero,
  ItemTypeTargetLanguage,
  ItemTypeAlwaysTranslateSource,
  ItemTypeNeverTranslateSource,
  ItemTypeNeverTranslateSite,
  ItemTypeTranslateButton,
};

@interface InfobarTranslateTableViewController ()

// InfobarTranslateModalDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarTranslateModalDelegate>
    infobarModalDelegate;

@end

@implementation InfobarTranslateTableViewController

- (instancetype)initWithDelegate:
    (id<InfobarTranslateModalDelegate>)modalDelegate {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
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

- (void)viewDidDisappear:(BOOL)animated {
  [self.infobarModalDelegate modalInfobarWasDismissed:self];
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

  TableViewTextEditItem* sourceLanguageItem = [self
      textEditItemForType:ItemTypeSourceLanguage
            textFieldName:
                l10n_util::GetNSString(
                    IDS_IOS_TRANSLATE_INFOBAR_MODAL_SOURCE_LANGUAGE_FIELD_NAME)
           textFieldValue:self.sourceLanguage];
  [model addItem:sourceLanguageItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextEditItem* targetLanguageItem = [self
      textEditItemForType:ItemTypeTargetLanguage
            textFieldName:
                l10n_util::GetNSString(
                    IDS_IOS_TRANSLATE_INFOBAR_MODAL_TARGET_LANGUAGE_FIELD_NAME)
           textFieldValue:self.targetLanguage];
  [model addItem:targetLanguageItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextButtonItem* translateButtonItem = [self
      textButtonItemForType:ItemTypeTranslateButton
                 buttonText:
                     l10n_util::GetNSString(
                         IDS_IOS_TRANSLATE_INFOBAR_NEVER_TRANSLATE_SITE_BUTTON_TITLE)];
  translateButtonItem.buttonTextColor = [UIColor colorNamed:kBlueColor];
  translateButtonItem.buttonBackgroundColor = [UIColor clearColor];
  [model addItem:translateButtonItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextButtonItem* alwaysTranslateSourceItem = [self
      textButtonItemForType:ItemTypeAlwaysTranslateSource
                 buttonText:
                     l10n_util::GetNSStringF(
                         IDS_IOS_TRANSLATE_INFOBAR_NEVER_TRANSLATE_SOURCE_BUTTON_TITLE,
                         base::SysNSStringToUTF16(self.sourceLanguage))];
  alwaysTranslateSourceItem.buttonTextColor = [UIColor colorNamed:kBlueColor];
  alwaysTranslateSourceItem.buttonBackgroundColor = [UIColor clearColor];
  [model addItem:alwaysTranslateSourceItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextButtonItem* neverTranslateSourceItem = [self
      textButtonItemForType:ItemTypeNeverTranslateSource
                 buttonText:
                     l10n_util::GetNSStringF(
                         IDS_IOS_TRANSLATE_INFOBAR_ALWAYS_TRANSLATE_SOURCE_BUTTON_TITLE,
                         base::SysNSStringToUTF16(self.sourceLanguage))];
  neverTranslateSourceItem.buttonTextColor = [UIColor colorNamed:kBlueColor];
  neverTranslateSourceItem.buttonBackgroundColor = [UIColor clearColor];
  [model addItem:neverTranslateSourceItem
      toSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextButtonItem* neverTranslateSiteItem =
      [self textButtonItemForType:ItemTypeNeverTranslateSite
                       buttonText:self.translateButtonText];
  neverTranslateSiteItem.disableButtonIntrinsicWidth = YES;
  [model addItem:neverTranslateSiteItem
      toSectionWithIdentifier:SectionIdentifierContent];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  // TODO(crbug.com/1014959): implement other button actions.
  if (itemType == ItemTypeTranslateButton) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button
               addTarget:self.infobarModalDelegate
                  action:@selector(modalInfobarButtonWasAccepted:)
        forControlEvents:UIControlEventTouchUpInside];
    tableViewTextButtonCell.selectionStyle = UITableViewCellSelectionStyleNone;
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return 0;
}

#pragma mark - Private

- (TableViewTextEditItem*)textEditItemForType:(ItemType)itemType
                                textFieldName:(NSString*)name
                               textFieldValue:(NSString*)value {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:itemType];
  item.textFieldEnabled = NO;
  item.textFieldName = name;
  item.textFieldValue = value;
  return item;
}

- (TableViewTextButtonItem*)textButtonItemForType:(ItemType)itemType
                                       buttonText:(NSString*)buttonText {
  TableViewTextButtonItem* item =
      [[TableViewTextButtonItem alloc] initWithType:itemType];
  item.boldButtonText = NO;
  item.buttonText = buttonText;
  return item;
}

- (void)dismissInfobarModal:(UIButton*)sender {
  // TODO(crbug.com/1014959): add metrics
  [self.infobarModalDelegate dismissInfobarModal:sender
                                        animated:YES
                                      completion:nil];
}

@end
