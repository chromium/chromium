// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator.h"

#import "base/check.h"
#import "base/notreached.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_country_selection_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator_delegate.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/utils/autofill_ai_entity_instance_builder.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

namespace {

// Returns an empty entity instance for the given type.
autofill::EntityInstance GetEmptyEntityInstanceForType(
    autofill::EntityType type) {
  autofill::EntityInstanceBuilder builder(type);

  // Iterate through all attributes for this specific type.
  for (autofill::AttributeType attr_type : type.attributes()) {
    builder.AddAttribute(autofill::AttributeInstance(attr_type));
  }

  return builder.Build();
}

// Returns the entity instance if `entity_id` is given.
// If `entity_id` is not given, then creates a new empty entity instance for
// the given type.
std::optional<autofill::EntityInstance> GetOrCreateEntityInstance(
    std::optional<autofill::EntityInstance::EntityId> entity_id,
    std::optional<autofill::EntityType> entity_type,
    autofill::EntityDataManager* data_manager) {
  if (entity_id.has_value()) {
    base::optional_ref<const autofill::EntityInstance> instance =
        data_manager->GetEntityInstance(*entity_id);
    if (!instance.has_value()) {
      return std::nullopt;
    }
    return *instance;
  } else {
    CHECK(entity_type.has_value());
    return GetEmptyEntityInstanceForType(*entity_type);
  }
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
  }
  return self;
}

- (void)start {
  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(
          self.browser->GetProfile());
  CHECK(entityDataManager);

  bool isNewEntity = !_entityID.has_value();
  std::optional<autofill::EntityInstance> instance = GetOrCreateEntityInstance(
      std::move(_entityID), std::move(_entityType), entityDataManager);
  if (!instance.has_value()) {
    [self.delegate autofillAIEntityEditCoordinatorDidFinish:self];
    return;
  }

  _mediator = [[AutofillAIEntityEditMediator alloc]
      initWithEntityInstance:std::move(*instance)
           entityDataManager:entityDataManager];

  _viewController = [[AutofillAIEntityEditTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;
  _viewController.mutator = _mediator;
  _viewController.startInEditMode = isNewEntity;

  _mediator.consumer = _viewController;

  CHECK(_baseNavigationController);
  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  _mediator = nil;

  if (_viewController) {
    _viewController.delegate = nil;
    if (_viewController.navigationController) {
      [_baseNavigationController popViewControllerAnimated:YES];
    }
    _viewController = nil;
  }
}

#pragma mark - AutofillAIEntityEditTableViewControllerDelegate

- (void)didTapCloseButton:
    (AutofillAIEntityEditTableViewController*)viewController {
  [self.delegate autofillAIEntityEditCoordinatorDidFinish:self];
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

  [_baseNavigationController pushViewController:countrySelectionController
                                       animated:YES];
}

#pragma mark - AutofillCountrySelectionTableViewControllerDelegate

- (void)didSelectCountry:(CountryItem*)selectedCountry {
  [_baseNavigationController popViewControllerAnimated:YES];
  [_mediator didSelectCountry:selectedCountry forItem:_countryItemBeingEdited];
  _countryItemBeingEdited = nil;
}

- (void)dismissCountryViewController {
  [_baseNavigationController popViewControllerAnimated:YES];
  _countryItemBeingEdited = nil;
}

@end
