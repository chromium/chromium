// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/public/autofill_ai_settings_constants.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_mutator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAttributes = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFooter = kItemTypeEnumZero,
};
}  // namespace

@interface AutofillAIEntityEditTableViewController () <
    TableViewTextEditItemDelegate>
@end

@implementation AutofillAIEntityEditTableViewController {
  // Items to be displayed.
  NSArray<TableViewItem*>* _editItems;

  // Whether editing is allowed.
  BOOL _editingAllowed;

  // Denotes the entity is saved in wallet.
  BOOL _isServerWalletItem;

  // The user's email address to display in the footer.
  NSString* _userEmail;

  // The bottom save button displayed when creating a new entity.
  ChromeButton* _saveButton;

  // The title text for the view.
  NSString* _titleText;

  // Whether the loading state is currently shown.
  BOOL _loadingState;

  // Whether `setEditItems:` has completed.
  BOOL _setEditItemsCompleted;
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
  [self validateFields];

  [self loadModel];

  // This is used to dismiss date picker UI when the user taps outside of it.
  UITapGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleTapOutside:)];
  // This allows the user to both dismiss a date picker and select another field
  // with a single tap.
  tapGesture.cancelsTouchesInView = NO;
  [self.view addGestureRecognizer:tapGesture];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierAttributes];

  for (TableViewItem* item in _editItems) {
    [model addItem:item toSectionWithIdentifier:SectionIdentifierAttributes];

    if ([item isKindOfClass:[AutofillAIEntityEditItem class]]) {
      AutofillAIEntityEditItem* editItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditItem>(item);
      editItem.textFieldEnabled = self.tableView.editing;
      editItem.hideIcon = !self.tableView.editing;
      editItem.textFieldDelegate = self;
      editItem.delegate = self;
    } else if ([item isKindOfClass:[AutofillAIEntityCountryItem class]]) {
      AutofillAIEntityCountryItem* countryItem =
          base::apple::ObjCCastStrict<AutofillAIEntityCountryItem>(item);
      [self updateAccessoryAndSelectionStyleForCountryItem:countryItem];
    } else if ([item isKindOfClass:[AutofillAIEntityEditDateItem class]]) {
      AutofillAIEntityEditDateItem* dateItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditDateItem>(item);
      dateItem.editingEnabled = self.tableView.editing;
      dateItem.hideIcon = !self.tableView.editing;
      dateItem.delegate = self;
      dateItem.textFieldDelegate = self;
    }
  }

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];

  if (_isServerWalletItem && _userEmail.length > 0) {
    footer.text = autofill::GetSaveEntityToWalletFooterText(_userEmail);
    footer.urls =
        @[ [[CrURL alloc] initWithGURL:autofill::GetManageYourInfoURL()] ];
  } else {
    footer.text =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SAVED_LOCALLY_FOOTER);
  }

  [model setFooter:footer forSectionWithIdentifier:SectionIdentifierAttributes];
}

#pragma mark - Setup

- (void)setupBottomSaveButton {
  _saveButton = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  _saveButton.title =
      l10n_util::GetNSString(autofill::GetSaveEntityAcceptButtonStringId());
  _saveButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_saveButton addTarget:self
                  action:@selector(didTapSaveNewEntity)
        forControlEvents:UIControlEventTouchUpInside];

  [self.view addSubview:_saveButton];

  [NSLayoutConstraint activateConstraints:@[
    [_saveButton.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [_saveButton.widthAnchor constraintEqualToAnchor:self.view.widthAnchor
                                            constant:-32],
    [_saveButton.bottomAnchor
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
  // Store a copy to ensure we can regenerate the title if `_isServerWalletItem`
  // is set after this method is called.
  _titleText = [title copy];
  BOOL shouldUseBrandedTitle =
      _isServerWalletItem && self.mode == AutofillAIEntityEditMode::kCreate;
  if (shouldUseBrandedTitle) {
    self.navigationItem.titleView =
        autofill::CreateBrandedTitleForWalletSave(title);
  } else {
    self.navigationItem.titleView = nil;
    super.title = title;
  }
}

- (void)setEditItems:(NSArray<TableViewItem*>*)items {
  _editItems = items;

  // If the view has already loaded, we need to reload the model to reflect
  // changes.
  if (self.isViewLoaded) {
    [self loadModel];
    [self.tableView reloadData];
  }
  [self validateFields];
  _setEditItemsCompleted = YES;
}

- (void)setEditingAllowed:(BOOL)editingAllowed {
  _editingAllowed = editingAllowed;
  [self updateUIForEditState];
}

- (void)setIsServerWalletItem:(BOOL)isServerWalletItem {
  _isServerWalletItem = isServerWalletItem;
  if (_titleText.length > 0) {
    [self setTitle:_titleText];
  }
}

- (void)setUserEmail:(NSString*)userEmail {
  _userEmail = [userEmail copy];
}

- (void)updateItem:(TableViewItem*)item {
  if ([item isKindOfClass:[AutofillAIEntityCountryItem class]]) {
    AutofillAIEntityCountryItem* countryItem =
        base::apple::ObjCCastStrict<AutofillAIEntityCountryItem>(item);
    [self updateAccessoryAndSelectionStyleForCountryItem:countryItem];
  }
  [self reconfigureCellsForItems:@[ item ]];
  [self validateFields];
}

- (void)reloadData {
  [self.tableView reloadData];
}

- (void)setLoadingState:(BOOL)loadingState {
  _loadingState = loadingState;

  [self validateFields];

  UIButtonConfiguration* buttonConfig = _saveButton.configuration;
  if (buttonConfig) {
    buttonConfig.showsActivityIndicator = _loadingState;
    _saveButton.configuration = buttonConfig;
  }

  // Prevent user from interacting with the form or dismissing the view in
  // loading state.
  self.tableView.userInteractionEnabled = !_loadingState;
  self.navigationItem.leftBarButtonItem.enabled = !_loadingState;

  // Prevent swipe-to-dismiss for modals in loading state.
  self.modalInPresentation = _loadingState;

  // Prevent edge-swipe back gesture in loading state.
  self.navigationController.interactivePopGestureRecognizer.enabled =
      !_loadingState;
}

- (void)didFinishSavingWithLocalFallback:(BOOL)isLocalFallback {
  if (isLocalFallback) {
    [self.delegate showLocalSaveFallbackAlert];
  } else if (self.mode == AutofillAIEntityEditMode::kCreate) {
    [self.delegate dismissViewController:self];
  }
}

#pragma mark - AutofillAIEntityEditDateItemDelegate

- (void)didChangeDate:(NSDate*)date
              forItem:(AutofillAIEntityEditDateItem*)item {
  [self.mutator didChangeDate:date forItem:item];
}

- (void)didDismissDateItem:(AutofillAIEntityEditDateItem*)item {
  [self.view endEditing:YES];
}

#pragma mark - Actions

- (void)handleTapOutside:(UITapGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateEnded) {
    // Whenever a user taps outside of the current field or date picker, stop
    // editing. This will dismiss the date picker UI used by date items.
    [self.view endEditing:YES];
  }
}

- (void)didTapCancel {
  [self.delegate dismissViewController:self];
}

- (void)didTapEdit {
  [self setEditing:!self.tableView.editing animated:YES];
}

- (void)didTapEditDone {
  if (![self validateFields]) {
    return;
  }
  [self.mutator saveEntityInstance];
  [self setEditing:NO animated:YES];
}

- (void)editButtonPressed {
  if (_isServerWalletItem) {
    [self.delegate didTapEditInWalletButton:self];
    return;
  }

  BOOL wasEditing = self.tableView.editing;
  if (wasEditing && ![self validateFields]) {
    return;
  }

  if (wasEditing) {
    [super editButtonPressed];
    if (!self.tableView.editing) {
      [self.mutator saveEntityInstance];
    }
  } else {
    __weak __typeof(self) weakSelf = self;
    [self.mutator
        requestEditingWithCompletion:^(ReauthenticationResult result) {
          if (result != ReauthenticationResult::kFailure) {
            [weakSelf onEditingRequestSucceeded];
          }
        }];
  }
}

- (void)didTapSaveNewEntity {
  CHECK(self.mode == AutofillAIEntityEditMode::kCreate);
  if (![self validateFields]) {
    return;
  }
  [self.mutator saveEntityInstance];
}

- (void)onEditingRequestSucceeded {
  [super editButtonPressed];
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

  if (_titleText.length > 0) {
    [self setTitle:_titleText];
  }
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

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  if (sectionIdentifier == SectionIdentifierAttributes) {
    TableViewLinkHeaderFooterView* linkView =
        base::apple::ObjCCast<TableViewLinkHeaderFooterView>(view);
    linkView.delegate = self;
  }
  return view;
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

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:
    (TableViewTextEditItem*)tableViewTextEditItem {
  if ([tableViewTextEditItem isKindOfClass:[AutofillAIEntityEditItem class]]) {
    AutofillAIEntityEditItem* editItem =
        base::apple::ObjCCastStrict<AutofillAIEntityEditItem>(
            tableViewTextEditItem);
    if (!editItem.hasValidValueStatus) {
      editItem.hasValidValueStatus = YES;
      [self reconfigureCellsForItems:@[ editItem ]];
    }
  }
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewTextEditItem {
  [self validateFields];
}

- (void)tableViewItemDidEndEditing:
    (TableViewTextEditItem*)tableViewTextEditItem {
  [self validateFields];

  // When a custom date picker is dismissed and editing ends, the date item'
  // text field must be reconfigured so that the edit icon is shown once again.
  if ([tableViewTextEditItem
          isKindOfClass:[AutofillAIEntityEditDateItem class]]) {
    [self reconfigureCellsForItems:@[ tableViewTextEditItem ]];
  }
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  // If the text field has a custom input view, block all direct keyboard input.
  return !textField.inputView;
}

- (UIMenu*)textField:(UITextField*)textField
    editMenuForCharactersInRange:(NSRange)range
                suggestedActions:(NSArray<UIMenuElement*>*)suggestedActions {
  // If the text field has a custom input view, prevent menu actions such as
  // "paste", "autofill" or "contacts" from showing up and writing data into the
  // text field.
  return [UIMenu menuWithTitle:@""
                      children:textField.inputView ? @[] : suggestedActions];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.delegate didTapLinkWithURL:URL];
}

#pragma mark - Private

- (autofill::DenseSet<autofill::AttributeType>)presentAttributes {
  autofill::DenseSet<autofill::AttributeType> present;
  for (TableViewItem* item in _editItems) {
    if ([item isKindOfClass:[AutofillAIEntityEditItem class]]) {
      AutofillAIEntityEditItem* editItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditItem>(item);
      if (editItem.textFieldValue.length > 0) {
        present.insert(autofill::AttributeType(editItem.attributeType));
      }
    } else if ([item isKindOfClass:[AutofillAIEntityCountryItem class]]) {
      AutofillAIEntityCountryItem* countryItem =
          base::apple::ObjCCastStrict<AutofillAIEntityCountryItem>(item);
      if (countryItem.detailText.length > 1) {
        present.insert(autofill::AttributeType(countryItem.attributeType));
      }
    } else if ([item isKindOfClass:[AutofillAIEntityEditDateItem class]]) {
      AutofillAIEntityEditDateItem* dateItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditDateItem>(item);
      if (dateItem.textFieldValue.length > 0) {
        present.insert(autofill::AttributeType(dateItem.attributeType));
      }
    }
  }
  return present;
}

- (autofill::DenseSet<autofill::AttributeType>)missingFields {
  const autofill::DenseSet<autofill::AttributeType> presentAttributes =
      [self presentAttributes];
  return [self.mutator getMissingImportConstraintsFor:presentAttributes];
}

- (BOOL)validateFields {
  const autofill::DenseSet<autofill::AttributeType> missingFields =
      [self missingFields];

  NSMutableArray<TableViewItem*>* itemsToReconfigure =
      [[NSMutableArray alloc] init];
  for (TableViewItem* item in _editItems) {
    if ([item isKindOfClass:[AutofillAIEntityEditItem class]]) {
      AutofillAIEntityEditItem* editItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditItem>(item);
      BOOL itemIsValid = !missingFields.contains(
          autofill::AttributeType(editItem.attributeType));
      if (!_setEditItemsCompleted) {
        itemIsValid = YES;
      }
      if (editItem.hasValidValueStatus != itemIsValid) {
        editItem.hasValidValueStatus = itemIsValid;
        [itemsToReconfigure addObject:editItem];
      }
    } else if ([item isKindOfClass:[AutofillAIEntityCountryItem class]]) {
      AutofillAIEntityCountryItem* countryItem =
          base::apple::ObjCCastStrict<AutofillAIEntityCountryItem>(item);
      BOOL itemIsValid = !missingFields.contains(
          autofill::AttributeType(countryItem.attributeType));
      if (!_setEditItemsCompleted) {
        itemIsValid = YES;
      }
      if (countryItem.hasValidValueStatus != itemIsValid) {
        countryItem.hasValidValueStatus = itemIsValid;
        [itemsToReconfigure addObject:countryItem];
      }
    } else if ([item isKindOfClass:[AutofillAIEntityEditDateItem class]]) {
      AutofillAIEntityEditDateItem* dateItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditDateItem>(item);
      BOOL itemIsValid = !missingFields.contains(
          autofill::AttributeType(dateItem.attributeType));
      if (!_setEditItemsCompleted) {
        itemIsValid = YES;
      }
      if (dateItem.hasValidValueStatus != itemIsValid) {
        dateItem.hasValidValueStatus = itemIsValid;
        [itemsToReconfigure addObject:dateItem];
      }
    }
  }
  if (itemsToReconfigure.count > 0) {
    [self reconfigureCellsForItems:itemsToReconfigure];
  }
  BOOL isValid = missingFields.empty();
  BOOL buttonEnabled = isValid && !_loadingState;
  if (self.mode == AutofillAIEntityEditMode::kCreate) {
    if (_saveButton) {
      _saveButton.enabled = buttonEnabled;
    }
  } else {
    self.navigationItem.rightBarButtonItem.enabled = buttonEnabled;
  }

  return isValid;
}

- (void)updateAccessoryAndSelectionStyleForCountryItem:
    (AutofillAIEntityCountryItem*)countryItem {
  if (self.tableView.editing) {
    countryItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    countryItem.editingAccessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
    countryItem.selectionStyle = UITableViewCellSelectionStyleDefault;
  } else {
    countryItem.accessoryType = UITableViewCellAccessoryNone;
    countryItem.editingAccessoryType = UITableViewCellAccessoryNone;
    countryItem.selectionStyle = UITableViewCellSelectionStyleNone;
  }
}

- (UIButton*)saveButton {
  return _saveButton;
}

@end
