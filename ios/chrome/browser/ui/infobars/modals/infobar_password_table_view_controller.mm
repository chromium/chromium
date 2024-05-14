// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_password_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_infobar_metrics_recorder.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_password_modal_delegate.h"
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
  ItemTypeSaveCredentials,
  ItemTypeCancel,
};

const CGFloat kSymbolSize = 15;
}  // namespace

@interface InfobarPasswordTableViewController () <UITextFieldDelegate>
// Properties backing InfobarPasswordModalConsumer interface.
@property(nonatomic, copy) NSString* username;
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
// Item that holds the SaveCredentials Button information.
@property(nonatomic, strong) TableViewTextButtonItem* saveCredentialsItem;
// Item that holds the cancel Button for this Infobar. e.g. "Never Save for this
// site".
@property(nonatomic, strong) TableViewTextButtonItem* cancelInfobarItem;
// Username at the time the InfobarModal is presented.
@property(nonatomic, copy) NSString* originalUsername;
// InfobarPasswordModalDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarPasswordModalDelegate>
    infobarModalDelegate;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;
// Used to build and record metrics specific to passwords.
@property(nonatomic, strong)
    IOSChromePasswordInfobarMetricsRecorder* passwordMetricsRecorder;
// Whether the current password being shown is masked or not.
@property(nonatomic, assign) BOOL passwordMasked;
@end

@implementation InfobarPasswordTableViewController

- (instancetype)initWithDelegate:(id<InfobarPasswordModalDelegate>)modalDelegate
                            type:(InfobarType)infobarType {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _infobarModalDelegate = modalDelegate;
    _metricsRecorder =
        [[InfobarMetricsRecorder alloc] initWithType:infobarType];
    switch (infobarType) {
      case InfobarType::kInfobarTypePasswordUpdate:
        _passwordMetricsRecorder =
            [[IOSChromePasswordInfobarMetricsRecorder alloc]
                initWithType:PasswordInfobarType::kPasswordInfobarTypeUpdate];
        break;
      case InfobarType::kInfobarTypePasswordSave:
        _passwordMetricsRecorder =
            [[IOSChromePasswordInfobarMetricsRecorder alloc]
                initWithType:PasswordInfobarType::kPasswordInfobarTypeSave];
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
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
                           action:@selector(dismissInfobarModal)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;

  UIImage* gearImage = DefaultSymbolWithPointSize(kSettingsFilledSymbol,
                                                  kInfobarSymbolPointSize);
  UIBarButtonItem* settingsButton = [[UIBarButtonItem alloc]
      initWithImage:gearImage
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(presentPasswordSettings)];
  settingsButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_MODAL_PASSWORD_SETTINGS_HINT);
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.rightBarButtonItem = settingsButton;
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

  TableViewTextEditItem* URLItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeURL];
  URLItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  URLItem.textFieldValue = self.URL;
  URLItem.hideIcon = YES;
  [model addItem:URLItem toSectionWithIdentifier:SectionIdentifierContent];

  self.originalUsername = self.username;
  self.usernameItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  self.usernameItem.fieldNameLabelText =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  self.usernameItem.textFieldValue = self.username;
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

  self.saveCredentialsItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSaveCredentials];
  self.saveCredentialsItem.textAlignment = NSTextAlignmentNatural;
  self.saveCredentialsItem.text = self.detailsTextMessage;
  self.saveCredentialsItem.buttonText = self.saveButtonText;
  self.saveCredentialsItem.enabled = !self.currentCredentialsSaved;
  self.saveCredentialsItem.disableButtonIntrinsicWidth = YES;
  [model addItem:self.saveCredentialsItem
      toSectionWithIdentifier:SectionIdentifierContent];

  if ([self.cancelButtonText length]) {
    self.cancelInfobarItem =
        [[TableViewTextButtonItem alloc] initWithType:ItemTypeCancel];
    self.cancelInfobarItem.buttonText = self.cancelButtonText;
    self.cancelInfobarItem.buttonTextColor = [UIColor colorNamed:kBlueColor];
    self.cancelInfobarItem.buttonBackgroundColor = [UIColor clearColor];
    self.cancelInfobarItem.boldButtonText = NO;
    [model addItem:self.cancelInfobarItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeSaveCredentials: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(saveCredentialsButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      tableViewTextButtonCell.separatorInset =
          UIEdgeInsetsMake(0, 0, 0, self.tableView.bounds.size.width);
      break;
    }
    case ItemTypeCancel: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(neverSaveCredentialsForCurrentSite)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeUsername: {
      TableViewTextEditCell* editCell =
          base::apple::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(usernameEditDidBegin)
                   forControlEvents:UIControlEventEditingDidBegin];
      [editCell.textField addTarget:self
                             action:@selector(updateSaveCredentialsButtonState)
                   forControlEvents:UIControlEventEditingChanged];
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypePassword: {
      TableViewTextEditCell* editCell =
          base::apple::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(updateSaveCredentialsButtonState)
                   forControlEvents:UIControlEventEditingChanged];
      [editCell.identifyingIconButton addTarget:self
                                         action:@selector(togglePasswordMasking)
                               forControlEvents:UIControlEventTouchUpInside];
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeURL:
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

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - Private Methods

- (void)updateSaveCredentialsButtonState {
  BOOL currentButtonState = [self.saveCredentialsItem isEnabled];
  BOOL newButtonState = [self.passwordItem.textFieldValue length] ? YES : NO;
  if (currentButtonState != newButtonState) {
    self.saveCredentialsItem.enabled = newButtonState;
    [self reconfigureCellsForItems:@[ self.saveCredentialsItem ]];
  }

  // TODO(crbug.com/40619978):Ideally the InfobarDelegate should update the
  // button text. Once we have a consumer protocol we should be able to create a
  // delegate that asks the InfobarDelegate for the correct text.
  NSString* buttonText =
      [self.usernameItem.textFieldValue isEqualToString:self.originalUsername]
          ? self.saveButtonText
          : l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON);
  if (![self.saveCredentialsItem.buttonText isEqualToString:buttonText]) {
    self.saveCredentialsItem.buttonText = buttonText;
    [self reconfigureCellsForItems:@[ self.saveCredentialsItem ]];
  }
}

- (void)dismissInfobarModal {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.infobarModalDelegate dismissInfobarModal:self];
}

- (void)saveCredentialsButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  if ([self.saveCredentialsItem.buttonText
          isEqualToString:l10n_util::GetNSString(
                              IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON)]) {
    [self.passwordMetricsRecorder
        recordModalDismiss:MobileMessagesPasswordsModalDismiss::
                               SavedCredentials];
  } else {
    [self.passwordMetricsRecorder
        recordModalDismiss:MobileMessagesPasswordsModalDismiss::
                               UpdatedCredentials];
  }
  [self.infobarModalDelegate
      updateCredentialsWithUsername:self.usernameItem.textFieldValue
                           password:self.unmaskedPassword];
}

- (void)presentPasswordSettings {
  base::RecordAction(base::UserMetricsAction("MobileMessagesModalSettings"));
  [self.metricsRecorder
      recordModalEvent:MobileMessagesModalEvent::SettingsOpened];
  [self.infobarModalDelegate presentPasswordSettings];
}

- (void)neverSaveCredentialsForCurrentSite {
  base::RecordAction(base::UserMetricsAction("MobileMessagesModalNever"));
  [self.passwordMetricsRecorder
      recordModalDismiss:MobileMessagesPasswordsModalDismiss::
                             TappedNeverForThisSite];
  [self.infobarModalDelegate neverSaveCredentialsForCurrentSite];
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
