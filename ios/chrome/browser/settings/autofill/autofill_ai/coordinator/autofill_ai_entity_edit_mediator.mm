// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_wallet_utils.h"
#import "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#import "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#import "components/autofill/core/browser/proto/server.pb.h"
#import "components/consent_auditor/consent_auditor.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/wallet/core/common/wallet_features.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_item_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_date_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_entity_instance_builder.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AttributeTypeName;
using autofill::EntityDataManager;
using autofill::EntityInstance;

namespace {

// Creates a date formatter for the given locale.
NSDateFormatter* CreateDateFormatterForLocale(const std::string& locale) {
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  // The desired format is the following: "Jun 10, 2024".
  // This maps to NSDateFormatterMediumStyle.
  dateFormatter.dateStyle = NSDateFormatterMediumStyle;
  dateFormatter.timeStyle = NSDateFormatterNoStyle;
  dateFormatter.locale =
      [NSLocale localeWithLocaleIdentifier:base::SysUTF8ToNSString(locale)];
  return dateFormatter;
}

// Logs metrics for entity save or update.
void LogEntitySaveOrUpdate(AutofillAIEntityEditMode mode,
                           const autofill::EntityInstance& entity) {
  if (mode == AutofillAIEntityEditMode::kCreate) {
    autofill::LogEntityAddedFromSettings(entity.type(), entity.record_type());
  } else {
    autofill::LogEntityUpdatedFromSettings(entity.type(), entity.record_type());
  }
}

}  // namespace

@implementation AutofillAIEntityEditMediator {
  // The entity instance being viewed and edited.
  std::optional<EntityInstance> _entityInstance;

  // The entity data manager. It outlives the mediator.
  raw_ptr<EntityDataManager> _entityDataManager;

  // The Wallet pass manager. It outlives the mediator.
  raw_ptr<autofill::WalletPassAccessManager> _walletPassManager;

  // The Consent Auditor. It outlives the mediator.
  raw_ptr<consent_auditor::ConsentAuditor> _consentAuditor;

  // The Identity Manager. It outlives the mediator.
  raw_ptr<signin::IdentityManager> _identityManager;

  // The locale used to get info from the entity instance.
  std::string _locale;

  // The date formatter used to display the date.
  NSDateFormatter* _dateFormatter;

  // Items to be displayed.
  NSMutableArray<TableViewItem*>* _editItems;

  // The fetched country list.
  NSArray<CountryItem*>* _allCountries;

  // The item factory.
  AutofillAIEntityEditItemFactory* _itemFactory;

  // The user's primary account email.
  NSString* _userEmail;

  // The reauthentication module.
  __weak id<ReauthenticationProtocol> _reauthModule;

  // Whether the view controller is currently in edit mode.
  BOOL _isEditing;
}

- (instancetype)
    initWithEntityInstance:(EntityInstance)entityInstance
         entityDataManager:(EntityDataManager*)entityDataManager
         walletPassManager:(autofill::WalletPassAccessManager*)walletPassManager
            consentAuditor:(consent_auditor::ConsentAuditor*)consentAuditor
           identityManager:(signin::IdentityManager*)identityManager
              reauthModule:(id<ReauthenticationProtocol>)reauthModule
                 userEmail:(NSString*)userEmail {
  self = [super init];
  if (self) {
    CHECK(reauthModule);

    _entityInstance = std::move(entityInstance);
    _entityDataManager = entityDataManager;
    _walletPassManager = walletPassManager;
    _consentAuditor = consentAuditor;
    _identityManager = identityManager;
    _locale = GetApplicationContext()->GetApplicationLocaleStorage()->Get();
    _dateFormatter = CreateDateFormatterForLocale(_locale);
    _itemFactory =
        [[AutofillAIEntityEditItemFactory alloc] initWithLocale:_locale
                                                  dateFormatter:_dateFormatter
                                           userHasAuthenticated:NO];
    _reauthModule = reauthModule;
    _userEmail = [userEmail copy];
  }
  return self;
}

// Sets the consumer of the mediator.
- (void)setConsumer:(id<AutofillAIEntityEditConsumer>)consumer {
  if (!consumer) {
    return;
  }

  _consumer = consumer;

  [self updateTitle];

  BOOL editingAllowed = !_entityInstance->are_attributes_read_only();
  BOOL isServerWalletItem = _entityInstance->record_type() ==
                            autofill::EntityInstance::RecordType::kServerWallet;
  // TODO(crbug.com/519241746): Remove this check when are_attributes_read_only
  // returns correct value for kOrder and kShipment.
  if (isServerWalletItem &&
      (_entityInstance->type().name() == autofill::EntityTypeName::kOrder ||
       _entityInstance->type().name() == autofill::EntityTypeName::kShipment)) {
    editingAllowed = NO;
  }
  [consumer setEditingAllowed:editingAllowed];
  [consumer setIsServerWalletItem:isServerWalletItem];
  [consumer setUserEmail:_userEmail];

  [self updateEditItemsWithAllAttributes:NO];
}

#pragma mark - AutofillAIEntityEditMutator

- (void)saveEntityInstance {
  CHECK(_entityInstance.has_value());
  _isEditing = NO;
  [self updateTitle];

  base::flat_set<autofill::AttributeInstance,
                 autofill::AttributeInstance::CompareByType>
      updatedAttributes;

  for (TableViewItem* item in _editItems) {
    if ([item isKindOfClass:[AutofillAIEntityEditItem class]]) {
      AutofillAIEntityEditItem* editItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditItem>(item);
      autofill::AttributeType attrType(editItem.attributeType);
      autofill::AttributeInstance attrInstance(attrType);
      attrInstance.SetInfo(attrType.field_type(),
                           base::SysNSStringToUTF16(editItem.textFieldValue),
                           _locale, std::nullopt,
                           autofill::VerificationStatus::kNoStatus);
      attrInstance.FinalizeInfo();
      updatedAttributes.insert(std::move(attrInstance));
    } else if ([item isKindOfClass:[AutofillAIEntityCountryItem class]]) {
      AutofillAIEntityCountryItem* countryItem =
          base::apple::ObjCCastStrict<AutofillAIEntityCountryItem>(item);
      autofill::AttributeType attrType(countryItem.attributeType);
      autofill::AttributeInstance attrInstance(attrType);
      attrInstance.SetInfo(attrType.field_type(),
                           base::SysNSStringToUTF16(countryItem.detailText),
                           _locale, std::nullopt,
                           autofill::VerificationStatus::kNoStatus);
      attrInstance.FinalizeInfo();
      updatedAttributes.insert(std::move(attrInstance));
    } else if ([item isKindOfClass:[AutofillAIEntityEditDateItem class]]) {
      AutofillAIEntityEditDateItem* dateItem =
          base::apple::ObjCCastStrict<AutofillAIEntityEditDateItem>(item);
      autofill::AttributeType attrType(dateItem.attributeType);
      autofill::AttributeInstance attrInstance(attrType);

      std::u16string value_to_save;
      if (dateItem.textFieldValue.length > 0 && dateItem.dateValue) {
        value_to_save = AttributeValueFromNSDate(dateItem.dateValue);
      }

      attrInstance.SetInfo(attrType.field_type(), value_to_save, _locale,
                           GetAttributeFormatString(),
                           autofill::VerificationStatus::kNoStatus);
      attrInstance.FinalizeInfo();
      updatedAttributes.insert(std::move(attrInstance));
    }
  }

  // If there are no attributes, we shouldn't save an empty entity.
  if (updatedAttributes.empty()) {
    return;
  }

  autofill::EntityInstanceBuilder builder(_entityInstance->type());
  builder.SetGUID(_entityInstance->guid())
      .SetNickname(_entityInstance->nickname())
      .SetDateModified(base::Time::Now())
      .SetUseCount(_entityInstance->use_count())
      .SetUseDate(_entityInstance->use_date())
      .SetRecordType(_entityInstance->record_type())
      .SetAreAttributesReadOnly(_entityInstance->are_attributes_read_only())
      .SetFrecencyOverride(std::string());

  for (const auto& attr : _entityInstance->attributes()) {
    builder.AddAttribute(attr);
  }
  for (const auto& attr : updatedAttributes) {
    builder.AddAttribute(attr);
  }

  _entityInstance = builder.Build();

  BOOL isWalletPrivatePass =
      autofill::GetWalletPassType(_entityInstance->type(),
                                  _entityInstance->record_type()) ==
      autofill::EntityInstance::WalletPassType::kPrivate;

  if (!isWalletPrivatePass || !_walletPassManager) {
    // Personal Context entities do not support saving.
    CHECK_NE(_entityInstance->record_type(),
             autofill::EntityInstance::RecordType::kPersonalContext);
    LogEntitySaveOrUpdate(self.consumer.mode, *_entityInstance);
    _entityDataManager->AddOrUpdateEntityInstance(*_entityInstance);
    [self.consumer didFinishSavingWithLocalFallback:NO];
    return;
  }

  if (![self.delegate mediator:self
          canPerformWalletSaveForType:_entityInstance->type()]) {
    // Save to local.
    EntityInstance local_entity = _entityInstance->CopyWithNewRecordType(
        EntityInstance::RecordType::kLocal);
    LogEntitySaveOrUpdate(self.consumer.mode, local_entity);
    _entityDataManager->AddOrUpdateEntityInstance(local_entity);
    [self.consumer didFinishSavingWithLocalFallback:YES];
    return;
  }

  [self.consumer setLoadingState:YES];

  autofill::EntityInstance originalEntity = *_entityInstance;

  consent_auditor::ConsentAuditor::SessionId sessionId;
  if (base::FeatureList::IsEnabled(
          wallet::features::kWalletApiPrivatePassesConsent)) {
    sessionId = autofill::RecordWalletPrivatePassConsent(
        /*consent_string_id=*/autofill::GetSaveToWalletSubtitleStringId(),
        /*clicked_button_string_id=*/
        autofill::GetSaveEntityAcceptButtonStringId(), *_consentAuditor,
        *_identityManager);
  }
    __weak __typeof(self) weakSelf = self;
    auto callback = base::BindOnce(
        [](__typeof(self) weakSelf,
           autofill::EntityInstance fallbackOriginalEntity,
           std::optional<autofill::EntityInstance> savedEntity) {
          [weakSelf
              onSavePrivatePassToWalletFinished:std::move(savedEntity)
                                 originalEntity:std::move(
                                                    fallbackOriginalEntity)];
        },
        weakSelf, std::move(originalEntity));

    _walletPassManager->SaveWalletEntityInstance(*_entityInstance, sessionId,
                                                 std::move(callback));
}

- (void)didChangeDate:(NSDate*)date
              forItem:(AutofillAIEntityEditDateItem*)item {
  item.dateValue = date;
  item.textFieldValue = [_dateFormatter stringFromDate:date];
  [self.consumer updateItem:item];
}

- (autofill::DenseSet<autofill::AttributeType>)getMissingImportConstraintsFor:
    (const autofill::DenseSet<autofill::AttributeType>&)presentAttributes {
  bool satisfied =
      std::ranges::any_of(_entityInstance->type().import_constraints(),
                          [&](const auto& constraint) {
                            return presentAttributes.contains_all(constraint);
                          });
  if (satisfied) {
    return {};
  }

  autofill::DenseSet<autofill::AttributeType> missingTypes;
  for (const auto& constraint : _entityInstance->type().import_constraints()) {
    for (auto type : constraint) {
      if (!presentAttributes.contains(type)) {
        missingTypes.insert(type);
      }
    }
  }
  return missingTypes;
}

- (void)requestEditingWithCompletion:(ReauthenticationResultBlock)completion {
  CHECK(completion);

  bool hasObfuscatedFields =
      std::ranges::any_of(_entityInstance->type().attributes(),
                          &autofill::AttributeType::is_obfuscated);
  if (!_entityInstance->IsMaskedEntity() && !hasObfuscatedFields) {
    _isEditing = YES;
    [self updateTitle];
    [self updateEditItemsWithAllAttributes:YES];
    completion(ReauthenticationResult::kSuccess);
    return;
  }

  NSString* reason = l10n_util::GetNSString(IDS_IOS_AUTH_REASON);
  __weak AutofillAIEntityEditMediator* weakSelf = self;
  [_reauthModule
      attemptReauthWithLocalizedReason:reason
                  canReusePreviousAuth:YES
                               handler:^(ReauthenticationResult result) {
                                 [weakSelf
                                     onReauthenticationFinished:
                                         result !=
                                         ReauthenticationResult::kFailure];
                                 completion(result);
                               }];
}

#pragma mark - Private

- (void)onReauthenticationFinished:(BOOL)success {
  if (success) {
    _isEditing = YES;
    [self updateTitle];
    [_itemFactory setUserHasAuthenticated:success];
    [self updateEditItemsWithAllAttributes:YES];
  }
}

- (void)updateTitle {
  if (!_consumer) {
    return;
  }

  autofill::EntityTypeName typeName = _entityInstance->type().name();
  NSString* title = nil;

  if (_consumer.mode == AutofillAIEntityEditMode::kCreate) {
    title = autofill::GetDialogTitleForAddEntity(typeName);
  } else if (_isEditing) {
    title = autofill::GetDialogTitleForEditEntity(typeName);
  } else {
    title = autofill::GetDialogTitleForViewEntity(typeName);
  }

  [_consumer setTitle:title];
}

- (void)updateEditItemsWithAllAttributes:(BOOL)showAllAttributes {
  _editItems = [[NSMutableArray alloc] init];
  if (showAllAttributes) {
    for (AttributeType attributeType : _entityInstance->type().attributes()) {
      base::optional_ref<const autofill::AttributeInstance> attribute =
          _entityInstance->attribute(attributeType);
      if (attribute.has_value()) {
        [_editItems addObject:[_itemFactory createItemForAttribute:*attribute]];
      } else {
        autofill::AttributeInstance emptyAttribute(attributeType);
        [_editItems
            addObject:[_itemFactory createItemForAttribute:emptyAttribute]];
      }
    }
  } else {
    for (AttributeInstance attribute : _entityInstance->attributes()) {
      [_editItems addObject:[_itemFactory createItemForAttribute:attribute]];
    }
  }

  [_consumer setEditItems:_editItems];
}

#pragma mark - Public

- (NSArray<CountryItem*>*)allCountries {
  if (!_allCountries) {
    _allCountries = [AutofillProfileEditMediator loadCountries];
  }
  return _allCountries;
}

- (void)didSelectCountry:(CountryItem*)countryItem
                 forItem:(AutofillAIEntityCountryItem*)item {
  item.detailText = countryItem.text;
  item.hasValidValueStatus = YES;
  [self.consumer updateItem:item];
}

- (GURL)walletManagementURL {
  CHECK(_entityInstance.has_value());
  return GURL(autofill::GetWalletManagementURL(*_entityInstance));
}

#pragma mark - Private

- (void)onSavePrivatePassToWalletFinished:
            (std::optional<autofill::EntityInstance>)savedEntity
                           originalEntity:
                               (autofill::EntityInstance)originalEntity {
  [self.consumer setLoadingState:NO];

  if (savedEntity.has_value()) {
    LogEntitySaveOrUpdate(self.consumer.mode, *savedEntity);
    _entityDataManager->AddOrUpdateEntityInstance(std::move(*savedEntity));
    [self.consumer didFinishSavingWithLocalFallback:NO];
  } else {
    // Wallet save failed, fallback to Local.
    autofill::EntityInstance localEntity = originalEntity.CopyWithNewRecordType(
        autofill::EntityInstance::RecordType::kLocal);
    LogEntitySaveOrUpdate(self.consumer.mode, localEntity);
    _entityDataManager->AddOrUpdateEntityInstance(std::move(localEntity));

    [self.consumer didFinishSavingWithLocalFallback:YES];
  }
}

@end
