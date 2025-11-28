// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_password_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_password_modal_delegate.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_infobar_metrics_recorder.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeURL = kItemTypeEnumZero,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeDetails,
};

const CGFloat kSymbolSize = 15;
}  // namespace

@interface InfobarPasswordTableViewController () <UITextFieldDelegate,
                                                  TableViewTextEditItemDelegate>
// Properties backing InfobarPasswordModalConsumer interface.
@property(nonatomic, copy) NSString* originalUsername;
@property(nonatomic, copy) NSString* maskedPassword;
@property(nonatomic, copy) NSString* unmaskedPassword;
@property(nonatomic, copy) NSString* detailsTextMessage;
@property(nonatomic, copy) NSString* URL;
@property(nonatomic, copy) NSString* saveButtonText;
@property(nonatomic, copy) NSString* cancelButtonText;
@property(nonatomic, assign) BOOL currentCredentialsSaved;
// Item that holds the Username TextField information.
@property(nonatomic, strong) TableViewTextEditItem* usernameItem;
// Item that holds the Password TextField information.
@property(nonatomic, strong) TableViewTextEditItem* passwordItem;
// Whether the current password being shown is masked or not.
@property(nonatomic, assign) BOOL passwordMasked;
@end

@implementation InfobarPasswordTableViewController {
  NSLayoutConstraint* _tableViewHeightConstraint;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  // The table view height will be set to its content view in
  // viewDidLayoutSubviews. Provide a default value here.
  _tableViewHeightConstraint = [self.tableView.heightAnchor
      constraintEqualToConstant:self.view.bounds.size.height];
  _tableViewHeightConstraint.active = YES;
  self.tableView.scrollEnabled = NO;
  self.tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  self.tableView.sectionHeaderHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  [self loadModel];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  _tableViewHeightConstraint.constant =
      self.tableView.contentSize.height +
      self.tableView.adjustedContentInset.top +
      self.tableView.adjustedContentInset.bottom;
}

#pragma mark - Properties

- (NSString*)username {
  return self.usernameItem.textFieldValue;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextEditItem* URLItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeURL];
  URLItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  URLItem.textFieldValue = self.URL;
  URLItem.hideIcon = YES;
  [model addItem:URLItem toSectionWithIdentifier:SectionIdentifierContent];

  self.usernameItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  self.usernameItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  self.usernameItem.textFieldValue = self.originalUsername;
  self.usernameItem.textFieldDelegate = self;
  self.usernameItem.returnKeyType = UIReturnKeyDone;
  self.usernameItem.textFieldEnabled = !self.currentCredentialsSaved;
  self.usernameItem.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  [model addItem:self.usernameItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.passwordItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypePassword];
  self.passwordItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  self.passwordItem.textFieldValue = self.maskedPassword;
  self.passwordItem.identifyingIcon =
      DefaultSymbolWithPointSize(kShowActionSymbol, kSymbolSize);
  self.passwordItem.identifyingIconEnabled = YES;
  self.passwordItem.hideIcon = YES;
  self.passwordItem.identifyingIconAccessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_INFOBAR_MODAL_PASSWORD_REVEAL_PASSWORD_HINT);
  [model addItem:self.passwordItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.passwordMasked = YES;

  TableViewDetailTextItem* detailItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeDetails];
  detailItem.allowMultilineDetailText = YES;
  detailItem.detailText = self.detailsTextMessage;
  [model addItem:detailItem toSectionWithIdentifier:SectionIdentifierContent];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeUsername: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypePassword: {
      TableViewTextEditCell* editCell =
          base::apple::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.identifyingIconButton addTarget:self
                                         action:@selector(togglePasswordMasking)
                               forControlEvents:UIControlEventTouchUpInside];
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeURL:
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    case ItemTypeDetails:
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return 0;
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  if (tableViewItem.type == ItemTypeUsername) {
    [self usernameEditDidBegin];
    return;
  }
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  if (tableViewItem.type == ItemTypeUsername) {
    [self updateSaveCredentialsButtonState];
    return;
  }
  if (tableViewItem.type == ItemTypePassword) {
    [self updateSaveCredentialsButtonState];
    return;
  }
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  // No op.
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - InfobarPasswordModalConsumer

- (void)setSaveButtonText:(NSString*)saveButtonText {
  [self.containerDelegate setAcceptButtonText:saveButtonText];
  _saveButtonText = [saveButtonText copy];
}

- (void)setCancelButtonText:(NSString*)cancelButtonText {
  [self.containerDelegate setCancelButtonText:cancelButtonText];
  _cancelButtonText = [cancelButtonText copy];
}

- (void)setCurrentCredentialsSaved:(BOOL)currentCredentialsSaved {
  [self.containerDelegate setCurrentCredentialsSaved:currentCredentialsSaved];
  _currentCredentialsSaved = currentCredentialsSaved;
}

#pragma mark - Private Methods

- (void)updateSaveCredentialsButtonState {
  BOOL saveEnabled = self.passwordItem.textFieldValue.length > 0;
  // TODO(crbug.com/40619978):Ideally the InfobarDelegate should update the
  // button text. Once we have a consumer protocol we should be able to create a
  // delegate that asks the InfobarDelegate for the correct text.
  NSString* buttonText =
      [self.usernameItem.textFieldValue isEqualToString:self.originalUsername]
          ? self.saveButtonText
          : l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON);

  [self.containerDelegate updateAcceptButtonEnabled:saveEnabled
                                              title:buttonText];
}

- (void)usernameEditDidBegin {
  [self.passwordMetricsRecorder
      recordModalEvent:MobileMessagesPasswordsModalEvent::EditedUserName];
}

- (void)togglePasswordMasking {
  self.passwordMasked = !self.passwordMasked;
  if (self.passwordMasked) {
    self.passwordItem.identifyingIcon =
        DefaultSymbolWithPointSize(kShowActionSymbol, kSymbolSize);
    self.passwordItem.textFieldValue = self.maskedPassword;
    self.passwordItem.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(
            IDS_IOS_INFOBAR_MODAL_PASSWORD_REVEAL_PASSWORD_HINT);
    [self.passwordMetricsRecorder
        recordModalEvent:MobileMessagesPasswordsModalEvent::MaskedPassword];
  } else {
    self.passwordItem.identifyingIcon =
        DefaultSymbolWithPointSize(kHideActionSymbol, kSymbolSize);
    self.passwordItem.textFieldValue = self.unmaskedPassword;
    self.passwordItem.identifyingIconAccessibilityLabel =
        l10n_util::GetNSString(
            IDS_IOS_INFOBAR_MODAL_PASSWORD_HIDE_PASSWORD_HINT);
    [self.passwordMetricsRecorder
        recordModalEvent:MobileMessagesPasswordsModalEvent::UnmaskedPassword];
  }
  [self reconfigureCellsForItems:@[ self.passwordItem ]];
}

@end
