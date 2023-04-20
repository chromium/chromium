// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/legacy_infobar_edit_address_profile_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/legacy_infobar_edit_address_profile_modal_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::AutofillTypeFromAutofillUIType;
using ::AutofillUITypeFromAutofillType;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
  SectionIdentifierButton
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeTextField = kItemTypeEnumZero,
  ItemTypeSaveButton,
};

}  // namespace

@interface LegacyInfobarEditAddressProfileTableViewController () <
    UITextFieldDelegate>

// The delegate passed to this instance.
@property(nonatomic, weak) id<LegacyInfobarEditAddressProfileModalDelegate>
    delegate;

// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// All the data to be displayed in the edit dialog.
@property(nonatomic, strong) NSMutableDictionary* profileData;

// Yes, if the edit is done for updating the profile.
@property(nonatomic, assign) BOOL isEditForUpdate;

@end

@implementation LegacyInfobarEditAddressProfileTableViewController

#pragma mark - Initialization

- (instancetype)initWithModalDelegate:
    (id<LegacyInfobarEditAddressProfileModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _delegate = modalDelegate;
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveAutofillAddressProfile];
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

  if (self.isEditForUpdate) {
    self.navigationItem.title =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
  } else {
    self.navigationItem.title =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }

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
    item.fieldNameLabelText = l10n_util::GetNSString(field.displayStringID);
    item.autofillUIType = AutofillUITypeFromAutofillType(field.autofillType);
    item.textFieldValue = _profileData[@(item.autofillUIType)];
    item.textFieldEnabled = YES;
    item.hideIcon = NO;
    item.autoCapitalizationType = field.autoCapitalizationType;
    item.returnKeyType = UIReturnKeyDone;
    item.keyboardType = field.keyboardType;
    [model addItem:item toSectionWithIdentifier:SectionIdentifierFields];
  }

  [model addSectionWithIdentifier:SectionIdentifierButton];
  TableViewTextButtonItem* saveButton =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSaveButton];
  saveButton.textAlignment = NSTextAlignmentNatural;
  if (self.isEditForUpdate) {
    saveButton.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  } else {
    saveButton.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }
  saveButton.disableButtonIntrinsicWidth = YES;
  [model addItem:saveButton toSectionWithIdentifier:SectionIdentifierButton];
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

- (void)setIsEditForUpdate:(BOOL)isEditForUpdate {
  _isEditForUpdate = isEditForUpdate;
}

- (void)setMigrationPrompt:(BOOL)migrationPrompt {
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
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.delegate dismissInfobarModal:self];
}

- (void)didTapSaveButton {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  [self updateProfileData];
  [self.delegate saveEditedProfileWithData:self.profileData];
}

#pragma mark - Private

- (void)updateProfileData {
  TableViewModel* model = self.tableViewModel;
  NSInteger section =
      [model sectionForSectionIdentifier:SectionIdentifierFields];
  NSInteger itemCount = [model numberOfItemsInSection:section];
  for (NSInteger itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
    NSIndexPath* path = [NSIndexPath indexPathForItem:itemIndex
                                            inSection:section];
    AutofillEditItem* item = base::mac::ObjCCastStrict<AutofillEditItem>(
        [model itemAtIndexPath:path]);
    self.profileData[[NSNumber numberWithInt:item.autofillUIType]] =
        item.textFieldValue;
  }
}

@end
