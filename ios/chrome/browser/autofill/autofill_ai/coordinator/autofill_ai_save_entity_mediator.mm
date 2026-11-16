// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_mediator.h"

#import <algorithm>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/timer/timer.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_consumer.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_prompt_model.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_observer_bridge.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"

// Time duration to wait before auto dismissing the bottomsheet in success
// confirmation state when VoiceOver is running. This is slightly greater than
// `kConfirmationStateDuration` to give VoiceOver enough time to read the
// required content.
// TODO(crbug.com/413009143): When VO is running do not use this and listen for
// VO announcement to finish before auto dismissing the bottomsheet in
// confirmation state.
static constexpr base::TimeDelta kConfirmationDismissDelayIfVoiceOverRunning =
    base::Seconds(5);

@interface AutofillAISaveEntityMediator () <
    IOSAutofillEntityDataManagerObserver> {
  std::unique_ptr<autofill::IOSAutofillEntityDataManagerObserverBridge>
      _entityDataManagerObserver;
}
@end

@implementation AutofillAISaveEntityMediator {
  raw_ptr<AutofillBottomSheetTabHelper> _tabHelper;
  std::optional<autofill::SaveEntityParams> _params;
  raw_ptr<autofill::EntityDataManager> _entityDataManager;
  // Timer that controls auto dismissal of the view controller after success
  // confirmation is shown.
  base::OneShotTimer _autoDismissConfirmationTimer;

  // URLs from the disclosure legal messages displayed in the footer.
  std::vector<GURL> _legalMessageURLs;

  // Whether `kLegalMessageShown` has already been logged for this presentation.
  BOOL _hasLoggedLegalMessageShown;

  BOOL _saveInProgress;
}

- (instancetype)initWithTabHelper:(AutofillBottomSheetTabHelper*)tabHelper
                entityDataManager:
                    (autofill::EntityDataManager*)entityDataManager {
  self = [super init];
  if (self) {
    CHECK(tabHelper);
    _tabHelper = tabHelper;
    _entityDataManager = entityDataManager;
    if (_entityDataManager) {
      _entityDataManagerObserver = std::make_unique<
          autofill::IOSAutofillEntityDataManagerObserverBridge>(
          _entityDataManager, self);
    }
  }
  return self;
}

- (instancetype)initWithParams:(autofill::SaveEntityParams)params
             entityDataManager:(autofill::EntityDataManager*)entityDataManager {
  self = [super init];
  if (self) {
    _params = std::move(params);
    _entityDataManager = entityDataManager;
    if (_entityDataManager) {
      _entityDataManagerObserver = std::make_unique<
          autofill::IOSAutofillEntityDataManagerObserverBridge>(
          _entityDataManager, self);
    }
  }
  return self;
}

- (void)setConsumer:(id<AutofillAISaveEntityConsumer>)consumer {
  _consumer = consumer;
  [self pushDataToConsumer];
}

- (void)didTapFooterLinkURL:(const GURL&)url {
  if (std::ranges::contains(_legalMessageURLs, url)) {
    autofill::LogWalletNoticeFunnelEvent(
        autofill::AutofillAiWalletNoticeFunnelEvents::kLinkClicked);
  }
}

- (void)disconnect {
  _consumer = nil;
  self.autofillHandler = nil;
  _saveInProgress = NO;
  _autoDismissConfirmationTimer.Stop();

  if (!base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn)) {
    if (_params && !_params->callback.is_null()) {
      std::move(_params->callback)
          .Run(autofill::AutofillClient::AutofillAiBubbleResult::kUnknown,
               std::nullopt, {});
    }
    _params = std::nullopt;
  }
  _entityDataManagerObserver.reset();
  _entityDataManager = nullptr;
  _tabHelper = nullptr;
}

#pragma mark - AutofillAISaveEntityMutator

- (void)acceptSaving {
  const autofill::SaveEntityParams* params = [self params];
  CHECK(params);

  // When `save_is_synchronous` is true, the profile is saved immediately.
  // Otherwise, the view enters the loading state and does an async call.
  if (params->save_is_synchronous) {
    [self saveEntity];
    // Dismiss immediately when it's a synchronous local save.
    [self.autofillHandler dismissSaveEntityDialog];
  } else {
    // The UI stays open to show the loading state. Once the async call is
    // completed, the UI is informed and the loading state is replaced by a
    // checkmark.
    [self.consumer showLoadingState];
    _saveInProgress = YES;
    [self saveEntity];
  }
}

- (void)cancelSaving {
  if (base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn)) {
    std::unique_ptr<AutofillAiSaveEntityPromptModel> model =
        _tabHelper->TakeAutofillAiSaveEntityPromptModel();
    if (model) {
      model->OnCanceled();
    }
  } else {
    if (_params && !_params->callback.is_null()) {
      std::move(_params->callback)
          .Run(autofill::AutofillClient::AutofillAiBubbleResult::kCancelled,
               std::nullopt, {});
    }
  }
  [self.autofillHandler dismissSaveEntityDialog];
}

- (void)dismissSaving {
  if (base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn)) {
    std::unique_ptr<AutofillAiSaveEntityPromptModel> model =
        _tabHelper->TakeAutofillAiSaveEntityPromptModel();
    if (model) {
      model->OnClosed();
    }
    [self.autofillHandler dismissSaveEntityDialog];
  } else {
    [self cancelSaving];
  }
}

#pragma mark - IOSAutofillEntityDataManagerObserver

- (void)onEntityInstancesChanged {
  if (!_saveInProgress) {
    return;
  }
  _saveInProgress = NO;

  [self showConfirmationForSave];
}

#pragma mark - Private

// Returns the active `SaveEntityParams` for UI presentation, depending on
// feature flag.
- (const autofill::SaveEntityParams*)params {
  if (base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn)) {
    AutofillAiSaveEntityPromptModel* model =
        _tabHelper->GetAutofillAiSaveEntityPromptModel();
    return model ? &model->params() : nullptr;
  }
  return _params ? &*_params : nullptr;
}

// Pushes the current data to the view controller.
- (void)pushDataToConsumer {
  const autofill::SaveEntityParams* params = [self params];
  if (!_consumer || !params) {
    return;
  }

  [_consumer
      setShowsExplicitDeclineButton:params->show_explicit_decline_button];

  [_consumer setNewEntity:params->new_entity
                oldEntity:params->old_entity
        saveIsSynchronous:params->save_is_synchronous];

  NSArray<AutofillLegalMessageLine*>* legalMessages =
      [AutofillLegalMessageLine convertFrom:params->public_passes_notice];
  autofill::AutofillAiEntityFooterContent footerContent =
      autofill::GetEntityFooterContent(
          params->new_entity, /*is_update=*/params->old_entity.has_value(),
          params->user_email, legalMessages);
  _legalMessageURLs = std::move(footerContent.legal_message_urls);

  if (!_legalMessageURLs.empty() && !_hasLoggedLegalMessageShown) {
    _hasLoggedLegalMessageShown = YES;
    autofill::LogWalletNoticeFunnelEvent(
        autofill::AutofillAiWalletNoticeFunnelEvents::kLegalMessageShown);
  }

  [_consumer setFooterText:footerContent.text urls:footerContent.urls];
}

- (BOOL)isSaveToWallet {
  const autofill::SaveEntityParams* params = [self params];
  return params && params->new_entity.record_type() ==
                       autofill::EntityInstance::RecordType::kServerWallet;
}

- (BOOL)isPrivateWalletPass {
  const autofill::SaveEntityParams* params = [self params];
  return params &&
         autofill::GetWalletPassType(params->new_entity.type(),
                                     params->new_entity.record_type()) ==
             autofill::EntityInstance::WalletPassType::kPrivate;
}

// Saves entity locally or to Google wallet.
- (void)saveEntity {
  autofill::AutofillClient::EntityImportUIContext UIContext =
      [self isSaveToWallet]
          ? autofill::AutofillClient::
                EntityImportUIContext{{autofill::
                                           GetSaveToWalletSubtitleStringId(
                                               [self isPrivateWalletPass])},
                                      autofill::
                                          GetSaveEntityAcceptButtonStringId()}
          : autofill::AutofillClient::EntityImportUIContext{};

  if (base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn)) {
    CHECK([self params]);
    std::unique_ptr<AutofillAiSaveEntityPromptModel> model =
        _tabHelper->TakeAutofillAiSaveEntityPromptModel();
    if (model) {
      model->OnAccepted(UIContext);
    }
  } else {
    CHECK(_params && !_params->callback.is_null());
    std::move(_params->callback)
        .Run(autofill::AutofillClient::AutofillAiBubbleResult::kAccepted,
             std::nullopt, UIContext);
  }
}

// Shows the checkmark and starts the dismissal timer.
- (void)showConfirmationForSave {
  [self.consumer showConfirmationState];

  __weak __typeof(self) weakSelf = self;
  _autoDismissConfirmationTimer.Start(
      FROM_HERE,
      UIAccessibilityIsVoiceOverRunning()
          ? kConfirmationDismissDelayIfVoiceOverRunning
          : kConfirmationDismissDelay,
      base::BindOnce(
          [](__weak __typeof(self) weakSelf) {
            [weakSelf dismissConfirmationStateOnTimeout];
          },
          weakSelf));
}

- (void)dismissConfirmationStateOnTimeout {
  _autoDismissConfirmationTimer.Stop();
  // Call the handler to dismiss the UI.
  [self.autofillHandler dismissSaveEntityDialog];
}

@end
