// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_mediator.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_modal/save_address_profile_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_edit_address_profile_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileModalRequestConfig;
using save_address_profile_infobar_modal_responses::CancelViewAction;
using save_address_profile_infobar_modal_responses::EditedProfileSaveAction;
using save_address_profile_infobar_modal_responses::NoThanksViewAction;

@interface SaveAddressProfileInfobarModalOverlayMediator ()
// The save address profile modal config from the request.
@property(nonatomic, assign, readonly)
    SaveAddressProfileModalRequestConfig* config;
// YES if the edit modal is being shown.
@property(nonatomic, assign) BOOL currentViewIsEditView;
@end

@implementation SaveAddressProfileInfobarModalOverlayMediator

#pragma mark - Accessors

- (SaveAddressProfileModalRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<SaveAddressProfileModalRequestConfig>()
             : nullptr;
}

- (void)setConsumer:(id<InfobarSaveAddressProfileModalConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;

  SaveAddressProfileModalRequestConfig* config = self.config;
  if (!_consumer || !config)
    return;

  NSDictionary* prefs = @{
    kAddressPrefKey : base::SysUTF16ToNSString(config->address()),
    kPhonePrefKey : base::SysUTF16ToNSString(config->phoneNumber()),
    kEmailPrefKey : base::SysUTF16ToNSString(config->emailAddress()),
    kCurrentAddressProfileSavedPrefKey :
        @(config->current_address_profile_saved()),
    kIsUpdateModalPrefKey : @(config->IsUpdateModal()),
    kProfileDataDiffKey : config->profile_diff(),
    kUpdateModalDescriptionKey :
        base::SysUTF16ToNSString(config->update_modal_description()),
    kIsMigrationToAccountKey : @(config->is_migration_to_account()),
    kUserEmailKey : config->user_email()
        ? base::SysUTF16ToNSString(config->user_email().value())
        : @"",
    kIsProfileAnAccountProfileKey : @(config->is_profile_an_account_profile()),
    kProfileDescriptionForMigrationPromptKey : base::SysUTF16ToNSString(
        config->profile_description_for_migration_prompt())
  };

  [_consumer setupModalViewControllerWithPrefs:prefs];
}

- (void)setEditAddressConsumer:
    (id<InfobarEditAddressProfileModalConsumer>)editAddressConsumer {
  if (_editAddressConsumer == editAddressConsumer)
    return;

  _editAddressConsumer = editAddressConsumer;

  SaveAddressProfileModalRequestConfig* config = self.config;
  if (!_editAddressConsumer || !config)
    return;

  [_editAddressConsumer setIsEditForUpdate:config->IsUpdateModal()];

  [_editAddressConsumer setMigrationPrompt:config->is_migration_to_account()];
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SaveAddressProfileModalRequestConfig::RequestSupport();
}

#pragma mark - InfobarSaveAddressProfileModalDelegate

- (void)showEditView {
  if (!self.request) {
    return;
  }

  self.currentViewIsEditView = YES;
  [self.saveAddressProfileMediatorDelegate showEditView];
}

- (void)noThanksButtonWasPressed {
  [self dispatchResponse:OverlayResponse::CreateWithInfo<NoThanksViewAction>()];
  [self dismissOverlay];
}

#pragma mark - InfobarEditAddressProfileModalDelegate

- (void)dismissInfobarModal:(id)infobarModal {
  base::RecordAction(base::UserMetricsAction(kInfobarModalCancelButtonTapped));

  // For migration prompt, the cancel from the edit view would result in removal
  // of the modal.
  if (self.config && self.config->is_migration_to_account() &&
      self.currentViewIsEditView) {
    [self noThanksButtonWasPressed];
    self.currentViewIsEditView = NO;
    return;
  }

  self.currentViewIsEditView = NO;
  [self dispatchResponse:OverlayResponse::CreateWithInfo<CancelViewAction>(
                             self.currentViewIsEditView)];
  [self dismissOverlay];
}

#pragma mark - Public

- (void)saveEditedProfileWithProfileData:(autofill::AutofillProfile*)profile {
  [self
      dispatchResponse:OverlayResponse::CreateWithInfo<EditedProfileSaveAction>(
                           profile)];
  [self dismissOverlay];
}

@end
