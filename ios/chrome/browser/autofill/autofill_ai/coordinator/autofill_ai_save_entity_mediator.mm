// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_mediator.h"

#import "base/timer/timer.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_consumer.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_observer_bridge.h"
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
  std::optional<autofill::SaveEntityParams> _params;
  raw_ptr<autofill::EntityDataManager> _entityDataManager;
  // Timer that controls auto dismissal of the view controller after success
  // confirmation is shown.
  base::OneShotTimer _autoDismissConfirmationTimer;

  BOOL _saveInProgress;
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

- (void)disconnect {
  _autoDismissConfirmationTimer.Stop();

  if (_params && !_params->callback.is_null()) {
    std::move(_params->callback)
        .Run(autofill::AutofillClient::AutofillAiBubbleResult::kUnknown,
             std::nullopt, {});
  }
  _params = std::nullopt;
  _consumer = nil;
}

#pragma mark - AutofillAISaveEntityMutator

- (void)acceptSaving {
  CHECK(_params);

  // When `_params->save_is_synchronous` is true, `_params->callback` is run
  // that saves the profile. Otherwise, the view enters the loading state and
  // `_params->callback` does an async call in the client.
  if (_params->save_is_synchronous) {
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
  if (_params && !_params->callback.is_null()) {
    std::move(_params->callback)
        .Run(autofill::AutofillClient::AutofillAiBubbleResult::kCancelled,
             std::nullopt, {});
  }
  [self.autofillHandler dismissSaveEntityDialog];
}

- (void)dismissSaving {
  [self cancelSaving];
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

// Pushes the current data to the view controller.
- (void)pushDataToConsumer {
  if (!_consumer || !_params) {
    return;
  }

  [_consumer setNewEntity:_params->new_entity
                oldEntity:_params->old_entity
                userEmail:_params->user_email
        saveIsSynchronous:_params->save_is_synchronous];
}

- (BOOL)isSaveToWallet {
  return _params->new_entity.record_type() ==
         autofill::EntityInstance::RecordType::kServerWallet;
}

// Saves entity locally or to Google wallet.
- (void)saveEntity {
  CHECK(!_params->callback.is_null());
  if ([self isSaveToWallet]) {
    std::move(_params->callback)
        .Run(autofill::AutofillClient::AutofillAiBubbleResult::kAccepted,
             std::nullopt,
             {autofill::GetSaveToWalletSubtitleStringId(),
              autofill::GetSaveEntityAcceptButtonStringId()});
  } else {
    std::move(_params->callback)
        .Run(autofill::AutofillClient::AutofillAiBubbleResult::kAccepted,
             std::nullopt, {});
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
