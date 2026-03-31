// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/public/autofill_ai_settings_constants.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_mutator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAttributes = kSectionIdentifierEnumZero,
};
}  // namespace

@implementation AutofillAIEntityEditTableViewController {
  // Items to be displayed.
  NSArray<TableViewItem*>* _editItems;

  // Whether editing is allowed.
  BOOL _editingAllowed;

  // Denotes the entity is saved in wallet.
  BOOL _isServerWalletItem;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kAutofillAIEntityEditTableViewId;
  self.shouldShowDeleteButtonInToolbar = YES;
  self.tableView.allowsSelectionDuringEditing = YES;

  if (self.mode == AutofillAIEntityEditMode::kCreate) {
    [self setEditing:YES animated:NO];
    self.shouldHideDoneButton = YES;
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                             target:self
                             action:@selector(didTapCancel)];
    [self setupBottomSaveButton];
  } else {
    self.navigationItem.rightBarButtonItem = [self editButtonItem];
  }

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierAttributes];

  for (TableViewItem* item in _editItems) {
    [model addItem:item toSectionWithIdentifier:SectionIdentifierAttributes];

    if ([item isKindOfClass:[AutofillAIEntityEditItem class]]) {
      AutofillAIEntityEditItem* editItem = (AutofillAIEntityEditItem*)item;
      editItem.textFieldEnabled = self.tableView.editing;
      editItem.hideIcon = !self.tableView.editing;
      editItem.textFieldDelegate = self;
    } else if ([item isKindOfClass:[AutofillAIEntityCountryItem class]]) {
      AutofillAIEntityCountryItem* countryItem =
          (AutofillAIEntityCountryItem*)item;
      if (self.tableView.editing) {
        countryItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        countryItem.selectionStyle = UITableViewCellSelectionStyleDefault;
      } else {
        countryItem.accessoryType = UITableViewCellAccessoryNone;
        countryItem.selectionStyle = UITableViewCellSelectionStyleNone;
      }
    } else if ([item isKindOfClass:[AutofillAIEntityEditDateItem class]]) {
      AutofillAIEntityEditDateItem* dateItem =
          (AutofillAIEntityEditDateItem*)item;
      dateItem.editingEnabled = self.tableView.editing;
      dateItem.delegate = self;
    }
  }
}

#pragma mark - Setup

- (void)setupBottomSaveButton {
  ChromeButton* saveButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  saveButton.title =
      l10n_util::GetNSString(IDS_IOS_SAVE_ENTITY_IN_SETTINGS_BUTTON_TEXT);
  saveButton.translatesAutoresizingMaskIntoConstraints = NO;
  [saveButton addTarget:self
                 action:@selector(didTapSaveNewEntity)
       forControlEvents:UIControlEventTouchUpInside];

  [self.view addSubview:saveButton];

  [NSLayoutConstraint activateConstraints:@[
    [saveButton.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [saveButton.widthAnchor constraintEqualToAnchor:self.view.widthAnchor
                                           constant:-32],
    [saveButton.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-16]
  ]];

  // Add bottom inset to the table view so the last cell doesn't get hidden
  // behind the button.
  self.tableView.contentInset = UIEdgeInsetsMake(0, 0, 80, 0);
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  if (self.mode == AutofillAIEntityEditMode::kCreate) {
    return NO;
  }
  return _editingAllowed || _isServerWalletItem;
}

- (BOOL)shouldShowEditDoneButton {
  // Only show the top right Done button if we are editing an existing entity.
  return self.mode == AutofillAIEntityEditMode::kViewAndEdit;
}

#pragma mark - AutofillAIEntityEditConsumer

- (void)setTitle:(NSString*)title {
  super.title = title;
}

- (void)setEditItems:(NSArray<TableViewItem*>*)items {
  _editItems = items;

  // If the view has already loaded, we need to reload the model to reflect
  // changes.
  if (self.isViewLoaded) {
    [self loadModel];
    [self.tableView reloadData];
  }
}

- (void)setEditingAllowed:(BOOL)editingAllowed {
  _editingAllowed = editingAllowed;
  [self updateUIForEditState];
}

- (void)setIsServerWalletItem:(BOOL)isServerWalletItem {
  _isServerWalletItem = isServerWalletItem;
}

- (void)updateItem:(TableViewItem*)item {
  [self reconfigureCellsForItems:@[ item ]];
}

- (void)reloadData {
  [self.tableView reloadData];
}

- (void)showLoadingState {
  // TODO(crbug.com/493915491): Implement loading state.
}

- (void)hideLoadingState {
  // TODO(crbug.com/493915491): Implement hiding of loading state.
}

- (void)didFinishSaving {
  [self.delegate didTapCloseButton:self];
}

#pragma mark - AutofillAIEntityEditDateItemDelegate

- (void)didChangeDate:(NSDate*)date
              forItem:(AutofillAIEntityEditDateItem*)item {
  [self.mutator didChangeDate:date forItem:item];
}

#pragma mark - Actions

- (void)didTapCancel {
  [self.delegate didTapCloseButton:self];
}

- (void)didTapEdit {
  [self setEditing:!self.tableView.editing animated:YES];
}

- (void)didTapEditDone {
  [self.mutator saveEntityInstance];
  [self setEditing:NO animated:YES];
}

- (void)editButtonPressed {
  if (_isServerWalletItem) {
    [self.delegate didTapEditInWalletButton:self];
    return;
  }

  BOOL wasEditing = self.tableView.editing;
  [super editButtonPressed];

  if (wasEditing && !self.tableView.editing) {
    [self.mutator saveEntityInstance];
  }
}

- (void)didTapSaveNewEntity {
  CHECK(self.mode == AutofillAIEntityEditMode::kCreate);
  [self.mutator saveEntityInstance];
}

#pragma mark -

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];

  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierAttributes]) {
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierAttributes];
  }
  [self loadModel];
  [self.tableView reloadData];
}

#pragma mark - UITableViewDelegate

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewCellEditingStyleNone;
}

- (BOOL)tableView:(UITableView*)tableView
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.tableView.editing) {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    if ([item isKindOfClass:[AutofillAIEntityCountryItem class]]) {
      AutofillAIEntityCountryItem* countryItem =
          base::apple::ObjCCast<AutofillAIEntityCountryItem>(item);
      [self.delegate didTapCountryItem:countryItem];
      [tableView deselectRowAtIndexPath:indexPath animated:YES];
    } else if ([item isKindOfClass:[AutofillAIEntityEditItem class]]) {
      UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
      if ([cell isKindOfClass:[TableViewTextEditCell class]]) {
        TableViewTextEditCell* textFieldCell =
            base::apple::ObjCCast<TableViewTextEditCell>(cell);
        [textFieldCell.textField becomeFirstResponder];
      }
    }
  }
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.tableView.editing) {
    return indexPath;
  }
  return [super tableView:tableView willSelectRowAtIndexPath:indexPath];
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  return self.tableView.editing;
}

#pragma mark - AutofillEditTableViewController

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:cellPath];
  return [item isKindOfClass:[AutofillAIEntityEditItem class]];
}

@end
