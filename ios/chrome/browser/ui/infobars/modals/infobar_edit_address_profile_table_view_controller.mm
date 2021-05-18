// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_edit_address_profile_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/stl_util.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillTypeFromAutofillUIType;
using ::AutofillUITypeFromAutofillType;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeTextField = kItemTypeEnumZero,
  ItemTypeSaveButton,
};

}  // namespace

@interface InfobarEditAddressProfileTableViewController () <UITextFieldDelegate>

// The delegate passed to this instance.
@property(nonatomic, weak) id<InfobarSaveAddressProfileModalDelegate> delegate;

// All the data to be displayed in the edit dialog.
@property(nonatomic, strong) NSMutableDictionary* profileData;

@end

@implementation InfobarEditAddressProfileTableViewController

#pragma mark - Initialization

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveAddressProfileModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _delegate = modalDelegate;
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.estimatedRowHeight = 56;

  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];

  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  // TODO(crbug.com/1167062): Replace with proper localized string.
  self.navigationItem.title = @"Test Edit Address";

  self.tableView.allowsSelectionDuringEditing = YES;

  [self loadModel];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierFields];
  for (const AutofillProfileFieldDisplayInfo& field : kProfileFieldsToDisplay) {
    if (field.autofillType == autofill::NAME_HONORIFIC_PREFIX &&
        !base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableSupportForHonorificPrefixes)) {
      continue;
    }

    AutofillEditItem* item =
        [[AutofillEditItem alloc] initWithType:ItemTypeTextField];
    item.textFieldName = l10n_util::GetNSString(field.displayStringID);
    item.autofillUIType = AutofillUITypeFromAutofillType(field.autofillType);
    item.textFieldValue = _profileData[@(item.autofillUIType)];
    item.textFieldEnabled = YES;
    item.hideIcon = NO;
    item.autoCapitalizationType = field.autoCapitalizationType;
    item.returnKeyType = field.returnKeyType;
    item.keyboardType = field.keyboardType;
    [model addItem:item toSectionWithIdentifier:SectionIdentifierFields];
  }

  TableViewTextButtonItem* saveButton =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSaveButton];
  saveButton.textAlignment = NSTextAlignmentNatural;
  // TODO(crbug.com/1167062): Replace with proper localized string.
  saveButton.buttonText = @"Test Save";
  saveButton.disableButtonIntrinsicWidth = YES;
  [model addItem:saveButton toSectionWithIdentifier:SectionIdentifierFields];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeTextField) {
    TableViewTextEditCell* editCell =
        base::mac::ObjCCastStrict<TableViewTextEditCell>(cell);
    editCell.textField.delegate = self;
    editCell.selectionStyle = UITableViewCellSelectionStyleNone;
  } else if (itemType == ItemTypeSaveButton) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button addTarget:self
                                       action:@selector(didTapSaveButton)
                             forControlEvents:UIControlEventTouchUpInside];
  }

  return cell;
}

#pragma mark - InfobarEditAddressProfileModalConsumer

- (void)setupModalViewControllerWithData:(NSDictionary*)data {
  self.profileData = [NSMutableDictionary dictionaryWithDictionary:data];
  [self.tableView reloadData];
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

#pragma mark - Actions

- (void)handleCancelButton {
  // TODO(crbug.com/1167062):  Implement the functionality.
}

- (void)didTapSaveButton {
  // TODO(crbug.com/1167062):  Implement the functionality.
}

@end
