// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/coordinator/manual_fill_address_mediator.h"

#import <memory>
#import <vector>

#import "base/check.h"
#import "base/check_op.h"
#import "base/containers/to_vector.h"
#import "base/functional/callback_helpers.h"
#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/browser/data_quality/addresses/address_import_requirement_util.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_address+AutofillProfile.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_address.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/address_consumer.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/address_list_delegate.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_address_cell.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using autofill::AutofillProfile;

namespace {

// Fetches the addresses to suggest using the given `personal_data_manager` and
// dereferences them before returning them.
std::vector<AutofillProfile> FetchAddresses(
    const autofill::PersonalDataManager& personal_data_manager) {
  std::vector<const AutofillProfile*> fetched_addresses =
      personal_data_manager.address_data_manager().GetProfilesToSuggest();
  // Make copies of the received `fetched_addresses` to not make any assumption
  // over their lifetime and make sure that the AutofillProfile objects stay
  // valid throughout the lifetime of this class.
  std::vector<AutofillProfile> addresses =
      base::ToVector(fetched_addresses,
                     [](const AutofillProfile* address) { return *address; });

  return addresses;
}

// Returns the value of `field_type` from `address`.
NSString* ValueForFieldType(ManualFillAddress* address,
                            ManualFillAddressFieldType field_type) {
  switch (field_type) {
    case ManualFillAddressFieldType::kFirstName:
      return address.firstName;
    case ManualFillAddressFieldType::kMiddleName:
      return address.middleNameOrInitial;
    case ManualFillAddressFieldType::kLastName:
      return address.lastName;
    case ManualFillAddressFieldType::kCompany:
      return address.company;
    case ManualFillAddressFieldType::kLine1:
      return address.line1;
    case ManualFillAddressFieldType::kLine2:
      return address.line2;
    case ManualFillAddressFieldType::kZip:
      return address.zip;
    case ManualFillAddressFieldType::kCity:
      return address.city;
    case ManualFillAddressFieldType::kState:
      return address.state;
    case ManualFillAddressFieldType::kCountry:
      return address.country;
    case ManualFillAddressFieldType::kPhoneNumber:
      return address.phoneNumber;
    case ManualFillAddressFieldType::kEmailAddress:
      return address.emailAddress;
  }
}

// Returns the user action metric corresponding to tapping `field_type`.
base::UserMetricsAction ActionForFieldType(
    ManualFillAddressFieldType field_type) {
  switch (field_type) {
    case ManualFillAddressFieldType::kFirstName:
      return base::UserMetricsAction("ManualFallback_Profiles_SelectFirstName");
    case ManualFillAddressFieldType::kMiddleName:
      return base::UserMetricsAction(
          "ManualFallback_Profiles_SelectMiddleName");
    case ManualFillAddressFieldType::kLastName:
      return base::UserMetricsAction("ManualFallback_Profiles_SelectLastName");
    case ManualFillAddressFieldType::kCompany:
      return base::UserMetricsAction("ManualFallback_Profiles_Company");
    case ManualFillAddressFieldType::kLine1:
      return base::UserMetricsAction("ManualFallback_Profiles_Address1");
    case ManualFillAddressFieldType::kLine2:
      return base::UserMetricsAction("ManualFallback_Profiles_Address2");
    case ManualFillAddressFieldType::kZip:
      return base::UserMetricsAction("ManualFallback_Profiles_Zip");
    case ManualFillAddressFieldType::kCity:
      return base::UserMetricsAction("ManualFallback_Profiles_City");
    case ManualFillAddressFieldType::kState:
      return base::UserMetricsAction("ManualFallback_Profiles_State");
    case ManualFillAddressFieldType::kCountry:
      return base::UserMetricsAction("ManualFallback_Profiles_Country");
    case ManualFillAddressFieldType::kPhoneNumber:
      return base::UserMetricsAction("ManualFallback_Profiles_PhoneNumber");
    case ManualFillAddressFieldType::kEmailAddress:
      return base::UserMetricsAction("ManualFallback_Profiles_EmailAddress");
  }
}

}  // namespace

@interface ManualFillAddressMediator () <PersonalDataManagerObserver>
@end

@implementation ManualFillAddressMediator {
  // All available addresses.
  std::vector<AutofillProfile> _addresses;

  // Personal data manager to be observed.
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  // C++ to ObjC bridge for PersonalDataManagerObserver.
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge>
      _personalDataManagerObserver;

  // Indicates whether to show the autofill button for the items.
  BOOL _showAutofillFormButton;

  // Service used to get the user's identity email address.
  raw_ptr<AuthenticationService> _authenticationService;
}

#pragma mark - Public

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                     showAutofillFormButton:(BOOL)showAutofillFormButton
                      authenticationService:
                          (AuthenticationService*)authenticationService {
  self = [super init];
  if (self) {
    _personalDataManager = personalDataManager;
    CHECK(_personalDataManager);
    _personalDataManagerObserver =
        std::make_unique<autofill::PersonalDataManagerObserverBridge>(
            _personalDataManager, self);
    _authenticationService = authenticationService;
    _addresses = FetchAddresses(*_personalDataManager);
    _showAutofillFormButton = showAutofillFormButton;
  }
  return self;
}

- (void)setConsumer:(id<ManualFillAddressConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self postAddressesToConsumer];
  [self postActionsToConsumer];
}

- (void)disconnect {
  _personalDataManagerObserver = nullptr;
  _personalDataManager = nullptr;
  _authenticationService = nullptr;
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  _addresses = FetchAddresses(*_personalDataManager);
  if (self.consumer) {
    [self postAddressesToConsumer];
    [self postActionsToConsumer];
  }
}

#pragma mark - ManualFillAddressCellMutator

- (void)didTapChipForAddress:(ManualFillAddress*)address
                   fieldType:(ManualFillAddressFieldType)fieldType {
  CHECK(address);
  base::RecordAction(ActionForFieldType(fieldType));
  [self.contentInjector
      userDidPickContent:ValueForFieldType(address, fieldType)
           passwordField:NO
           requiresHTTPS:NO
         jumpToNextField:YES
              actionType:autofill::mojom::FieldActionType::kReplaceAll];
}

- (void)didTapAutofillFormButtonForAddress:(ManualFillAddress*)address
                                 cellIndex:(NSInteger)cellIndex {
  CHECK(address);
  CHECK_GE(cellIndex, 0);
  base::UmaHistogramSparse(
      "Autofill.UserAcceptedSuggestionAtIndex.Address.ManualFallback",
      cellIndex);
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Profiles_SuggestionAccepted"));

  FormSuggestion* suggestion = [FormSuggestion
              suggestionWithValue:nil
                       minorValue:nil
               displayDescription:nil
                             icon:nil
                             type:autofill::SuggestionType::kAddressEntry
                          payload:autofill::Suggestion::AutofillProfilePayload(
                                      autofill::Suggestion::Guid(
                                          base::SysNSStringToUTF8(
                                              address.GUID)))
      fieldByFieldFillingTypeUsed:autofill::EMPTY_TYPE
                   requiresReauth:NO
       acceptanceA11yAnnouncement:l10n_util::GetNSString(
                                      IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM)];

  [self.contentInjector autofillFormWithSuggestion:suggestion
                                           atIndex:cellIndex];
}

#pragma mark - Private

// Posts the addresses to the consumer.
- (void)postAddressesToConsumer {
  if (!self.consumer) {
    return;
  }

  int addressCount = _addresses.size();
  NSMutableArray* items =
      [[NSMutableArray alloc] initWithCapacity:addressCount];
  for (int i = 0; i < addressCount; i++) {
    ManualFillAddress* manualFillAddress =
        [[ManualFillAddress alloc] initWithProfile:_addresses[i]];

    NSArray<UIAction*>* menuActions =
        @[ [self createMenuEditActionForAddress:_addresses[i]] ];

    NSString* cellIndexAccessibilityLabel = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_IOS_MANUAL_FALLBACK_ADDRESS_CELL_INDEX),
            "count", addressCount, "position", i + 1));

    auto item = [[ManualFillAddressItem alloc]
                    initWithAddress:manualFillAddress
                            mutator:self
                        menuActions:menuActions
                          cellIndex:static_cast<NSInteger>(i)
        cellIndexAccessibilityLabel:cellIndexAccessibilityLabel
             showAutofillFormButton:_showAutofillFormButton];
    [items addObject:item];
  }

  [self.consumer presentAddresses:items];
}

- (void)postActionsToConsumer {
  if (!self.consumer) {
    return;
  }

  NSString* manageAddressesTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_MANAGE_ADDRESSES_AND_MORE);
  __weak __typeof(self) weakSelf = self;
  ManualFillActionItem* manageAddressesItem = [[ManualFillActionItem alloc]
      initWithTitle:manageAddressesTitle
             action:^{
               base::RecordAction(base::UserMetricsAction(
                   "ManualFallback_Profiles_OpenManageProfiles"));
               [weakSelf.navigationDelegate openAddressSettings];
             }];
  manageAddressesItem.accessibilityIdentifier =
      manual_fill::kManageAddressAccessibilityIdentifier;
  [self.consumer presentActions:@[ manageAddressesItem ]];
}

// Creates a UIAction to edit an address from a UIMenu.
- (UIAction*)createMenuEditActionForAddress:(const AutofillProfile)address {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:
          kMenuScenarioHistogramAutofillManualFallbackAddressEntry];

  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(
      [](__weak __typeof(self) weak_self, AutofillProfile address) {
        [weak_self openAddressDetailsInEditMode:std::move(address)];
      },
      weakSelf, std::move(address));
  UIAction* editAction = [actionFactory
      actionToEditWithBlock:base::CallbackToBlock(std::move(callback))];

  return editAction;
}

// Requests the `navigationDelegate` to open the details of the given `address`
// in edit mode.
- (void)openAddressDetailsInEditMode:(AutofillProfile)address {
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Profiles_OverflowMenu_Edit"));
  BOOL offerMigrateToAccount = [self offerMigrateToAccountForAddress:address];
  [self.navigationDelegate openAddressDetailsInEditMode:std::move(address)
                                  offerMigrateToAccount:offerMigrateToAccount];
}

// Evaluates whether or not the option to move the address to the account should
// be available when navigating to the details page of the given address.
- (BOOL)offerMigrateToAccountForAddress:(const AutofillProfile&)address {
  BOOL addressIsEligibleForAccountMigration = IsEligibleForMigrationToAccount(
      _personalDataManager->address_data_manager(), address);
  BOOL userEmailIsValid = [self userEmail] != nil;

  return addressIsEligibleForAccountMigration && userEmailIsValid;
}

// Returns the user's identity email address.
- (NSString*)userEmail {
  id<SystemIdentity> identity = _authenticationService->GetPrimaryIdentity();
  if (identity) {
    return identity.userEmail;
  } else {
    return nil;
  }
}

@end
