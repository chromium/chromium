// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator.h"

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/model/autofill_ai_error_dialog_context.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/autofill/model/ios_wallet_pass_access_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_country_selection_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/consent_auditor/model/consent_auditor_factory.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator_delegate.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_entity_instance_builder.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace {

// Returns an empty entity instance for the given type.
autofill::EntityInstance GetEmptyEntityInstanceForType(
    autofill::EntityType type,
    autofill::EntityInstance::RecordType record_type) {
  autofill::EntityInstanceBuilder builder(type);
  builder.SetRecordType(record_type);

  // Iterate through all attributes for this specific type.
  for (autofill::AttributeType attr_type : type.attributes()) {
    builder.AddAttribute(autofill::AttributeInstance(attr_type));
  }

  return builder.Build();
}

}  // namespace

@interface AutofillAIEntityEditCoordinator () <
    AutofillAIEntityEditTableViewControllerDelegate,
    AutofillCountrySelectionTableViewControllerDelegate>

@property(nonatomic, strong) AutofillAIEntityEditMediator* mediator;

@end

@implementation AutofillAIEntityEditCoordinator {
  // The base navigation controller.
  UINavigationController* _baseNavigationController;

  // The entity ID.
  std::optional<autofill::EntityInstance::EntityId> _entityID;

  // The entity type for a new entity.
  std::optional<autofill::EntityType> _entityType;

  // The view controller.
  AutofillAIEntityEditTableViewController* _viewController;

  // The item being edited for country selection.
  AutofillAIEntityCountryItem* _countryItemBeingEdited;

  // Denotes if a new entity is being added or an existing one is edited.
  AutofillAIEntityEditMode _editMode;
}

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        entityID:
                                            (autofill::EntityInstance::EntityId)
                                                entityID {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _entityID = entityID;
    _editMode = AutofillAIEntityEditMode::kViewAndEdit;
  }
  return self;
}

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                      entityType:
                                          (autofill::EntityType)entityType {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _entityType = entityType;
    _editMode = AutofillAIEntityEditMode::kCreate;
  }
  return self;
}

- (void)start {
  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(
          self.browser->GetProfile());
  CHECK(entityDataManager);

  std::optional<autofill::EntityInstance> instance =
      [self getOrCreateEntityInstanceWithDataManager:*entityDataManager];

  if (!instance.has_value()) {
    [self.delegate autofillAIEntityEditCoordinatorDidFinish:self];
    return;
  }

  autofill::WalletPassAccessManager* walletPassManager =
      IOSWalletPassAccessManagerFactory::GetForProfile(
          self.browser->GetProfile());

  ReauthenticationService* reauthService =
      ReauthenticationServiceFactory::GetForProfile(self.browser->GetProfile());

  _mediator = [[AutofillAIEntityEditMediator alloc]
      initWithEntityInstance:std::move(*instance)
           entityDataManager:entityDataManager
           walletPassManager:walletPassManager
              consentAuditor:ConsentAuditorFactory::GetForProfile(
                                 self.browser->GetProfile())
             identityManager:IdentityManagerFactory::GetForProfile(
                                 self.browser->GetProfile())
                reauthModule:reauthService->GetReauthModule()
                   userEmail:[self userEmail]];

  _viewController = [[AutofillAIEntityEditTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;
  _viewController.mutator = _mediator;
  _viewController.mode = _editMode;

  _mediator.consumer = _viewController;

  CHECK(_baseNavigationController);
  if (_editMode == AutofillAIEntityEditMode::kCreate) {
    // Present modally for creating a new entity.
    UINavigationController* navController = [[UINavigationController alloc]
        initWithRootViewController:_viewController];
    [_baseNavigationController presentViewController:navController
                                            animated:YES
                                          completion:nil];
  } else {
    // Push for viewing an existing entity.
    [_baseNavigationController pushViewController:_viewController animated:YES];
  }
}

- (void)stop {
  _mediator = nil;

  if (_viewController) {
    _viewController.delegate = nil;

    if (_editMode == AutofillAIEntityEditMode::kCreate) {
      [_viewController.navigationController dismissViewControllerAnimated:YES
                                                               completion:nil];
    } else if (_viewController.navigationController) {
      [_baseNavigationController popViewControllerAnimated:YES];
    }
    _viewController = nil;
  }
}

#pragma mark - AutofillAIEntityEditTableViewControllerDelegate

- (void)dismissViewController:
    (AutofillAIEntityEditTableViewController*)viewController {
  [self.delegate autofillAIEntityEditCoordinatorDidFinish:self];
}

- (void)didTapLinkWithURL:(CrURL*)url {
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:url.gurl]];
}

- (void)didTapCountryItem:(AutofillAIEntityCountryItem*)item {
  _countryItemBeingEdited = item;
  NSString* countryValue = item.detailText;

  AutofillCountrySelectionTableViewController* countrySelectionController =
      [[AutofillCountrySelectionTableViewController alloc]
                     initWithDelegate:self
                      selectedCountry:countryValue
                         allCountries:_mediator.allCountries
                         settingsView:YES
          previousViewControllerTitle:_viewController.title];

  [_viewController.navigationController
      pushViewController:countrySelectionController
                animated:YES];
}

- (void)didTapEditInWalletButton:
    (AutofillAIEntityEditTableViewController*)viewController {
  GURL walletURL = [_mediator walletManagementURL];

  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:walletURL]];
}

- (void)showLocalSaveFallbackAlert {
  CHECK(_editMode == AutofillAIEntityEditMode::kCreate);
  id<AutofillCommands> autofillHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AutofillCommands);

  autofill::AutofillAiErrorDialogContext errorContext;
  errorContext.type = autofill::AutofillAiErrorDialogType::kTypeLocalSave;
  errorContext.show_immediately = true;

  __weak __typeof(self) weakSelf = self;
  errorContext.on_dismissed_callback = base::BindOnce(^{
    [weakSelf.delegate autofillAIEntityEditCoordinatorDidFinish:weakSelf];
  });

  [autofillHandler showAutofillAiErrorDialog:std::move(errorContext)];
}

#pragma mark - AutofillCountrySelectionTableViewControllerDelegate

- (void)didSelectCountry:(CountryItem*)selectedCountry {
  [_viewController.navigationController popViewControllerAnimated:YES];
  [_mediator didSelectCountry:selectedCountry forItem:_countryItemBeingEdited];
  _countryItemBeingEdited = nil;
}

- (void)dismissCountryViewController {
  [_viewController.navigationController popViewControllerAnimated:YES];
  _countryItemBeingEdited = nil;
}

#pragma mark - Private

// Fetches the email associated with the user account.
- (NSString*)userEmail {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.browser->GetProfile());
  CoreAccountInfo accountInfo =
      identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  return base::SysUTF8ToNSString(accountInfo.email);
}

// Returns a new empty entity instance for `_entityType` if `_editMode` is
// `kCreate`. Otherwise, returns the existing entity instance for `_entityID`.
- (std::optional<autofill::EntityInstance>)
    getOrCreateEntityInstanceWithDataManager:
        (const autofill::EntityDataManager&)entityDataManager {
  if (_editMode == AutofillAIEntityEditMode::kCreate) {
    CHECK(_entityType.has_value());

    // Default to local.
    autofill::EntityInstance::RecordType targetRecordType =
        autofill::EntityInstance::RecordType::kLocal;

    if (autofill::CanPerformAutofillAiAction(
            self.browser->GetProfile(),
            autofill::AutofillAiAction::kImportToWallet, _entityType)) {
      targetRecordType = autofill::EntityInstance::RecordType::kServerWallet;
    }

    return GetEmptyEntityInstanceForType(*_entityType, targetRecordType);
  } else {
    CHECK(_entityID.has_value());

    // Fetch the existing entity.
    base::optional_ref<const autofill::EntityInstance> existingInstance =
        entityDataManager.GetEntityInstance(*_entityID);

    if (existingInstance.has_value()) {
      return *existingInstance;
    }

    return std::nullopt;
  }
}

@end
