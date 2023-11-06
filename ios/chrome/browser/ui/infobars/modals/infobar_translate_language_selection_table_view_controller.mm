// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_language_selection_table_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_language_selection_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_modal_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

@interface InfobarTranslateLanguageSelectionTableViewController ()

// Stores the items received from
// setTranslateLanguageItems:forChangingSourceLanguage: to populate the
// TableViewModel in loadModel.
@property(nonatomic, strong) NSArray<TableViewTextItem*>* modelItems;

// YES if this ViewController is displaying language options to change change
// the source language. NO if it is displaying language options to change the
// target language.
@property(nonatomic, assign) BOOL selectingSourceLanguage;

// The InfobarTranslateLanguageSelectionDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarTranslateLanguageSelectionDelegate>
    langageSelectionDelegate;

@end

@implementation InfobarTranslateLanguageSelectionTableViewController

- (instancetype)initWithDelegate:(id<InfobarTranslateLanguageSelectionDelegate>)
                                     langageSelectionDelegate
         selectingSourceLanguage:(BOOL)sourceLanguage {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _langageSelectionDelegate = langageSelectionDelegate;
    _selectingSourceLanguage = sourceLanguage;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];
  self.tableView.accessibilityIdentifier =
      kTranslateInfobarLanguageSelectionTableViewAXId;
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];
  for (TableViewTextItem* item in self.modelItems) {
    [self.tableViewModel addItem:item
         toSectionWithIdentifier:SectionIdentifierContent];
  }
}

#pragma mark - InfobarTranslateLanguageSelectionConsumer

- (void)setTranslateLanguageItems:(NSArray<TableViewTextItem*>*)items {
  // If this is called after viewDidLoad/loadModel, then a [self.tableView
  // reloadData] call will be needed or else the items displayed won't be
  // updated.
  self.modelItems = items;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  // All items should be a TableViewTextItem and of just one type
  // kItemTypeEnumZero. They are populated in the mediator with this assumption.
  TableViewTextItem* item = static_cast<TableViewTextItem*>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  if (item.checked) {
    cell.userInteractionEnabled = NO;
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // All items should be a TableViewTextItem and of just one type
  // kItemTypeEnumZero. They are populated in the mediator with this assumption.
  TableViewTextItem* item = static_cast<TableViewTextItem*>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  DCHECK(item.type == kItemTypeEnumZero);
  if (self.selectingSourceLanguage) {
    [self.langageSelectionDelegate didSelectSourceLanguageIndex:indexPath.row
                                                       withName:item.text];
  } else {
    [self.langageSelectionDelegate didSelectTargetLanguageIndex:indexPath.row
                                                       withName:item.text];
  }
  [self.navigationController popViewControllerAnimated:YES];
}

@end
