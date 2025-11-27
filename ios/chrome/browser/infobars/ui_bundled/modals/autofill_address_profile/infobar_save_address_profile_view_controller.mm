// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/ui_bundled/modals/autofill_address_profile/infobar_save_address_profile_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/autofill_address_profile/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_address_profile_modal_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFields = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSaveAddressField = kItemTypeEnumZero,
  ItemTypeMigrateInAccountAddress,
  ItemTypeUpdateAddressField,
  ItemTypeUpdateModalDescription,
  ItemTypeUpdateModalTitle,
  ItemTypeAddressProfileSaveUpdateButton,
  ItemTypeAddressProfileNoThanksButton,
  ItemTypeFooter,
  ItemTypeNotFound
};

const CGFloat kSymbolSize = 16;

// Defines the separator inset value for this table view.
const CGFloat kSeparatorInset = 60;

const CGFloat kButtonVerticalInset = 9;
const CGFloat kButtonHorizontalInset = 16;

}  // namespace

@interface InfobarSaveAddressProfileViewController ()

// InfobarSaveAddressProfileModalDelegate for this ViewController.
@property(nonatomic, weak) id<InfobarSaveAddressProfileModalDelegate>
    saveAddressProfileModalDelegate;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// Item for displaying and editing the address.
@property(nonatomic, copy) NSString* address;
// Item for displaying and editing the phone number.
@property(nonatomic, copy) NSString* phoneNumber;
// Item for displaying and editing the email address.
@property(nonatomic, copy) NSString* emailAddress;
// YES if the Address Profile being displayed has been saved.
@property(nonatomic, assign) BOOL currentAddressProfileSaved;
// Yes, if the update address profile modal is to be displayed.
@property(nonatomic, assign) BOOL isUpdateModal;
// Contains the content for the update modal.
@property(nonatomic, copy) NSDictionary* profileDataDiff;
// Description of the update modal.
@property(nonatomic, copy) NSString* updateModalDescription;
// Stores the user email for the currently signed-in account.
@property(nonatomic, copy) NSString* userEmail;
// If YES, denotes that the profile will be added to the Google Account.
@property(nonatomic, assign) BOOL isMigrationToAccount;
// IF YES, for update prompt, the profile belongs to the Google Account.
// For save prompt, denotes that the profile will be saved to Google Account.
@property(nonatomic, assign) BOOL profileAnAccountProfile;
// If YES, the update prompt is shown for home profile.
@property(nonatomic, assign) BOOL homeProfile;
// If YES, the update prompt is shown for work profile.
@property(nonatomic, assign) BOOL workProfile;
// Description shown in the migration prompt.
@property(nonatomic, copy) NSString* profileDescriptionForMigrationPrompt;

@end

@implementation InfobarSaveAddressProfileViewController {
  UIStackView* _contentStack;
}

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveAddressProfileModalDelegate>)modalDelegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _saveAddressProfileModalDelegate = modalDelegate;
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveAutofillAddressProfile];
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)loadView {
  self.view = [[UIScrollView alloc] init];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  if (!self.isMigrationToAccount || self.currentAddressProfileSaved) {
    // Do not show the cancel button when the migration prompt is presented and
    // the profile is not migrated yet.
    UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(dismissInfobarModal)];
    cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;
    self.navigationItem.leftBarButtonItem = cancelButton;
  }

  if (!self.currentAddressProfileSaved) {
    UIBarButtonItem* editButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemEdit
                             target:self
                             action:@selector(showEditAddressProfileModal)];
    editButton.accessibilityIdentifier = kInfobarSaveAddressModalEditButton;
    self.navigationItem.rightBarButtonItem = editButton;
  }

  self.navigationController.navigationBar.prefersLargeTitles = NO;

  self.title = [self computeTitle];

  [self loadContent];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
  [super viewDidDisappear:animated];
}

#pragma mark - InfobarSaveAddressProfileModalConsumer

- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs {
  self.address = prefs[kAddressPrefKey];
  self.phoneNumber = prefs[kPhonePrefKey];
  self.emailAddress = prefs[kEmailPrefKey];
  self.currentAddressProfileSaved =
      [prefs[kCurrentAddressProfileSavedPrefKey] boolValue];
  self.isUpdateModal = [prefs[kIsUpdateModalPrefKey] boolValue];
  self.profileDataDiff = prefs[kProfileDataDiffKey];
  self.updateModalDescription = prefs[kUpdateModalDescriptionKey];
  self.isMigrationToAccount = [prefs[kIsMigrationToAccountKey] boolValue];
  self.userEmail = prefs[kUserEmailKey];
  self.profileAnAccountProfile =
      [prefs[kIsProfileAnAccountProfileKey] boolValue];
  self.homeProfile = [prefs[kIsProfileAnAccountHomeKey] boolValue];
  self.workProfile = [prefs[kIsProfileAnAccountWorkKey] boolValue];
  self.profileDescriptionForMigrationPrompt =
      prefs[kProfileDescriptionForMigrationPromptKey];
  [self loadViewIfNeeded];
  [self loadContent];
}

#pragma mark - Actions

- (void)saveAddressProfileButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  [self.saveAddressProfileModalDelegate modalInfobarButtonWasAccepted:self];
}

- (void)dismissInfobarModal {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.saveAddressProfileModalDelegate dismissInfobarModal:self];
}

- (void)showEditAddressProfileModal {
  [self.saveAddressProfileModalDelegate dismissInfobarModal:self];
  [self.saveAddressProfileModalDelegate showEditView];
}

- (void)noThanksButtonWasPressed:(UIButton*)sender {
  [self.saveAddressProfileModalDelegate noThanksButtonWasPressed];
}

#pragma mark - Private Methods

- (void)loadContent {
  CHECK(self.isViewLoaded);
  [_contentStack removeFromSuperview];

  _contentStack = [[UIStackView alloc] init];
  _contentStack.axis = UILayoutConstraintAxisVertical;
  _contentStack.alignment = UIStackViewAlignmentFill;
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_contentStack];
  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor
        constraintEqualToAnchor:_contentStack.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:_contentStack.trailingAnchor],
    [self.view.topAnchor constraintEqualToAnchor:_contentStack.topAnchor],
    [self.view.bottomAnchor constraintEqualToAnchor:_contentStack.bottomAnchor],
    [self.view.widthAnchor constraintEqualToAnchor:_contentStack.widthAnchor],
  ]];

  if (self.isMigrationToAccount) {
    [self loadMigrationToAccountModal];
  } else if (self.isUpdateModal) {
    [self loadUpdateAddressModal];
  } else {
    [self loadSaveAddressModal];
  }
}

- (void)loadUpdateAddressModal {
  CHECK_GT([self.profileDataDiff count], 0ul);

  [_contentStack addArrangedSubview:[self updateModalDescriptionView]];

  BOOL showOld = [self shouldShowOldSection];

  if (showOld) {
    [_contentStack
        addArrangedSubview:
            [self
                titleWithText:
                    l10n_util::GetNSString(
                        IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_NEW_VALUES_SECTION_LABEL)]];
  }

  for (NSNumber* type in self.profileDataDiff) {
    if ([self.profileDataDiff[type][0] length] > 0) {
      UIView* newValue =
          [self detailViewWithTitle:self.profileDataDiff[type][0]
                             symbol:[self symbolForUpdateModelFromAutofillType:
                                              static_cast<autofill::FieldType>(
                                                  [type intValue])]
               imageTintColorIsGrey:NO];
      [_contentStack addArrangedSubview:newValue];
      [_contentStack addArrangedSubview:[self separatorView]];
    }
  }

  if (showOld) {
    [_contentStack
        addArrangedSubview:
            [self
                titleWithText:
                    l10n_util::GetNSString(
                        IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OLD_VALUES_SECTION_LABEL)]];
    for (NSNumber* type in self.profileDataDiff) {
      if ([self.profileDataDiff[type][1] length] > 0) {
        UIView* oldValue = [self
             detailViewWithTitle:self.profileDataDiff[type][1]
                          symbol:[self symbolForUpdateModelFromAutofillType:
                                           static_cast<autofill::FieldType>(
                                               [type intValue])]
            imageTintColorIsGrey:YES];
        [_contentStack addArrangedSubview:oldValue];
        [_contentStack addArrangedSubview:[self separatorView]];
      }
    }
  }

  if (self.profileAnAccountProfile) {
    [_contentStack addArrangedSubview:[self updateFooter]];
  }

  [_contentStack addArrangedSubview:[self saveButton]];
}

- (void)loadSaveAddressModal {
  UIView* address = [self detailViewWithTitle:self.address
                                       symbol:CustomSymbolTemplateWithPointSize(
                                                  kLocationSymbol, kSymbolSize)
                         imageTintColorIsGrey:YES];
  [_contentStack addArrangedSubview:address];
  [_contentStack addArrangedSubview:[self separatorView]];

  if ([self.emailAddress length]) {
    UIView* email =
        [self detailViewWithTitle:self.emailAddress
                           symbol:DefaultSymbolTemplateWithPointSize(
                                      kMailFillSymbol, kSymbolSize)
             imageTintColorIsGrey:YES];
    [_contentStack addArrangedSubview:email];
    [_contentStack addArrangedSubview:[self separatorView]];
  }

  if ([self.phoneNumber length]) {
    UIView* phone =
        [self detailViewWithTitle:self.phoneNumber
                           symbol:DefaultSymbolTemplateWithPointSize(
                                      kPhoneFillSymbol, kSymbolSize)
             imageTintColorIsGrey:YES];
    [_contentStack addArrangedSubview:phone];
    [_contentStack addArrangedSubview:[self separatorView]];
  }

  if (self.isMigrationToAccount || self.profileAnAccountProfile) {
    [_contentStack addArrangedSubview:[self saveFooter]];
  }

  [_contentStack addArrangedSubview:[self saveButton]];
}

- (void)loadMigrationToAccountModal {
  [_contentStack addArrangedSubview:[self migrationPromptFooter]];

  [_contentStack
      addArrangedSubview:
          [self detailViewWithTitle:self.profileDescriptionForMigrationPrompt
                             symbol:CustomSymbolTemplateWithPointSize(
                                        kLocationSymbol, kSymbolSize)
               imageTintColorIsGrey:YES]];
  [_contentStack addArrangedSubview:[self separatorView]];

  [_contentStack addArrangedSubview:[self saveButton]];

  if (!self.currentAddressProfileSaved) {
    [_contentStack addArrangedSubview:[self noThanksButton]];
  }
}

- (UIView*)saveButton {
  ChromeButton* button =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  NSString* title;
  if (self.isMigrationToAccount) {
    title = l10n_util::GetNSString(
        IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_OK_BUTTON_LABEL);
  } else if (self.isUpdateModal) {
    int buttonTextId = IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL;
    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableSupportForHomeAndWork)) {
      if (self.homeProfile || self.workProfile) {
        buttonTextId = IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL;
      } else if (![self shouldShowOldSection]) {
        buttonTextId =
            IDS_AUTOFILL_UPDATE_ADDRESS_ADD_NEW_INFO_PROMPT_OK_BUTTON_LABEL;
      }
    }
    title = l10n_util::GetNSString(buttonTextId);
  } else {
    title = l10n_util::GetNSString(
        IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }
  button.title = title;

  [button addTarget:self
                action:@selector(saveAddressProfileButtonWasPressed:)
      forControlEvents:UIControlEventTouchUpInside];

  return [self buttonContainerWithButton:button];
}

- (UIView*)noThanksButton {
  ChromeButton* button =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStyleSecondary];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.title = l10n_util::GetNSString(
      IDS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_CANCEL_BUTTON_LABEL);

  [button addTarget:self
                action:@selector(noThanksButtonWasPressed:)
      forControlEvents:UIControlEventTouchUpInside];

  return [self buttonContainerWithButton:button];
}

- (UIView*)buttonContainerWithButton:(UIView*)button {
  UIView* buttonContainer = [[UIView alloc] init];
  [buttonContainer addSubview:button];

  [NSLayoutConstraint activateConstraints:@[
    [button.topAnchor constraintEqualToAnchor:buttonContainer.topAnchor
                                     constant:kButtonVerticalInset],
    [button.bottomAnchor constraintEqualToAnchor:buttonContainer.bottomAnchor
                                        constant:-kButtonVerticalInset],
    [button.leadingAnchor constraintEqualToAnchor:buttonContainer.leadingAnchor
                                         constant:kButtonHorizontalInset],
    [button.trailingAnchor
        constraintEqualToAnchor:buttonContainer.trailingAnchor
                       constant:-kButtonHorizontalInset],
  ]];
  return buttonContainer;
}

- (UIView*)titleWithText:(NSString*)text {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.attributedTitle = [[NSAttributedString alloc]
      initWithString:text
          attributes:@{
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
          }];

  return [configuration makeAccessibilityConfiguredContentView];
}

- (UIView*)updateModalDescriptionView {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.subtitle = self.updateModalDescription;
  return [configuration makeAccessibilityConfiguredContentView];
}

// Returns the symbol corresponding to the `type` for the update modal view.
- (UIImage*)symbolForUpdateModelFromAutofillType:(autofill::FieldType)type {
  switch (GetAddressUIComponentIconTypeForFieldType(type)) {
    case autofill::AddressUIComponentIconType::kNoIcon:
      return nil;
    case autofill::AddressUIComponentIconType::kName:
      return DefaultSymbolTemplateWithPointSize(kPersonFillSymbol, kSymbolSize);
    case autofill::AddressUIComponentIconType::kAddress:
      return CustomSymbolTemplateWithPointSize(kLocationSymbol, kSymbolSize);
    case autofill::AddressUIComponentIconType::kEmail:
      return DefaultSymbolTemplateWithPointSize(kMailFillSymbol, kSymbolSize);
    case autofill::AddressUIComponentIconType::kPhone:
      return DefaultSymbolTemplateWithPointSize(kPhoneFillSymbol, kSymbolSize);
  }
}

// Returns YES if the old section is shown in the update modal.
- (BOOL)shouldShowOldSection {
  // Determines whether the old section is to be shown or not.
  for (NSNumber* type in self.profileDataDiff) {
    if ([self.profileDataDiff[type][1] length] > 0) {
      return YES;
    }
  }

  return NO;
}

// Returns the title of the view.
- (NSString*)computeTitle {
  if (self.isMigrationToAccount) {
    return l10n_util::GetNSString(
        IDS_IOS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_TITLE);
  }

  if (!self.isUpdateModal) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }

  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSupportForHomeAndWork)) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
  }

  if (self.homeProfile || self.workProfile) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }

  return l10n_util::GetNSString(
      [self shouldShowOldSection]
          ? IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE
          : IDS_IOS_AUTOFILL_ADD_NEW_INFO_ADDRESS_PROMPT_TITLE);
}

// Returns a separator view.
- (UIView*)separatorView {
  UIView* separatorContainer = [[UIView alloc] init];
  UIView* separator = [[UIView alloc] init];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];

  [separatorContainer addSubview:separator];
  [NSLayoutConstraint activateConstraints:@[
    [separator.heightAnchor constraintEqualToConstant:0.7],
    [separator.topAnchor constraintEqualToAnchor:separatorContainer.topAnchor],
    [separator.leadingAnchor
        constraintEqualToAnchor:separatorContainer.leadingAnchor
                       constant:kSeparatorInset],
    [separator.trailingAnchor
        constraintEqualToAnchor:separatorContainer.trailingAnchor],
    [separator.bottomAnchor
        constraintEqualToAnchor:separatorContainer.bottomAnchor],
  ]];
  return separatorContainer;
}

#pragma mark - Item Constructors

- (UIView*)detailViewWithTitle:(NSString*)title
                        symbol:(UIImage*)symbol
          imageTintColorIsGrey:(BOOL)imageTintColorIsGrey {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = title;

  if (symbol) {
    ColorfulSymbolContentConfiguration* symbolConfiguration =
        [[ColorfulSymbolContentConfiguration alloc] init];
    symbolConfiguration.symbolImage =
        CustomSymbolTemplateWithPointSize(kLocationSymbol, kSymbolSize);
    symbolConfiguration.symbolTintColor =
        imageTintColorIsGrey ? [UIColor colorNamed:kGrey400Color]
                             : [UIColor colorNamed:kBlueColor];

    configuration.leadingConfiguration = symbolConfiguration;
  }

  return [configuration makeAccessibilityConfiguredContentView];
}

- (UIView*)saveFooter {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  int footerTextId = self.currentAddressProfileSaved
                         ? IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT
                         : IDS_IOS_AUTOFILL_SAVE_ADDRESS_IN_ACCOUNT_FOOTER;
  CHECK_GT([self.userEmail length], 0ul);
  configuration.subtitle = l10n_util::GetNSStringF(
      footerTextId, base::SysNSStringToUTF16(self.userEmail));

  return [configuration makeAccessibilityConfiguredContentView];
}

- (UIView*)updateFooter {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  int footerTextId =
      self.homeProfile
          ? IDS_AUTOFILL_ADDRESS_HOME_RECORD_TYPE_NOTICE
          : (self.workProfile
                 ? IDS_AUTOFILL_ADDRESS_WORK_RECORD_TYPE_NOTICE
                 : IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT);
  CHECK_GT([self.userEmail length], 0ul);
  configuration.subtitle = l10n_util::GetNSStringF(
      footerTextId, base::SysNSStringToUTF16(self.userEmail));

  return [configuration makeAccessibilityConfiguredContentView];
}

- (UIView*)migrationPromptFooter {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  int footerTextId = self.currentAddressProfileSaved
                         ? IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT
                         : IDS_IOS_AUTOFILL_ADDRESS_MIGRATE_IN_ACCOUNT_FOOTER;
  CHECK_GT([self.userEmail length], 0ul);
  configuration.subtitle = l10n_util::GetNSStringF(
      footerTextId, base::SysNSStringToUTF16(self.userEmail));

  return [configuration makeAccessibilityConfiguredContentView];
}

@end
