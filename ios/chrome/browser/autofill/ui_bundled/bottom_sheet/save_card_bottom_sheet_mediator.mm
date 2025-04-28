// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mediator.h"

#import <memory>
#import <optional>
#import <utility>

#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

// Bridges the C++ Observer interface to the Objective-C mediator. Scoped
// observation adds and removes this bridge as the observer of the
// SaveCardBottomSheetModel.
class ModelObserverBridge
    : public autofill::SaveCardBottomSheetModel::Observer {
 public:
  ModelObserverBridge(autofill::SaveCardBottomSheetModel* model,
                      __weak SaveCardBottomSheetMediator* mediator)
      : mediator_(mediator) {
    scoped_model_observation_.Observe(model);
  }
  ~ModelObserverBridge() override = default;
  void OnCreditCardUploadCompleted(bool card_saved) override {
    [mediator_ onCreditCardUploadCompleted:card_saved];
  }

 private:
  __weak SaveCardBottomSheetMediator* mediator_;
  base::ScopedObservation<autofill::SaveCardBottomSheetModel,
                          ModelObserverBridge>
      scoped_model_observation_{this};
};

// Time duration to wait before auto dismissing the bottomsheet in success
// confirmation state when VoiceOver is running. This is slightly greater than
// `kConfirmationStateDuration` to give VoiceOver enough time to read the
// required content.
// TODO(crbug.com/413009143): When VO is running do not use this and listen for
// VO announcement to finish before auto dismissing the bottomsheet in
// confirmation state.
static constexpr base::TimeDelta kConfirmationDismissDelayIfVoiceOverRunning =
    base::Seconds(5);

}  // namespace

// TODO(crbug.com/402511942): Implement SaveCardBottomSheetMediator.
@implementation SaveCardBottomSheetMediator {
  // `_modelObserverBridge` holds a scoped observation of the model for the
  // mediator and must be destroyed before the `_saveCardBottomSheetModel`

  // The model layer component providing resources and callbacks for
  // saving the card or rejecting the card upload.
  std::unique_ptr<autofill::SaveCardBottomSheetModel> _saveCardBottomSheetModel;
  __weak id<AutofillCommands> _autofillCommandsHandler;
  std::optional<ModelObserverBridge> _modelObserverBridge;

  // Timer that controls auto dismissal of bottomsheet after success
  // confirmation is shown.
  base::OneShotTimer _autoDismissConfirmationTimer;

  // Indicates whether bottom sheet is already in the process of dismissing.
  BOOL _dismissing;
}

- (instancetype)initWithUIModel:
                    (std::unique_ptr<autofill::SaveCardBottomSheetModel>)model
        autofillCommandsHandler:(id<AutofillCommands>)autofillCommandsHandler {
  self = [super init];
  if (self) {
    _saveCardBottomSheetModel = std::move(model);
    _autofillCommandsHandler = autofillCommandsHandler;
    _modelObserverBridge.emplace(_saveCardBottomSheetModel.get(), self);
  }
  return self;
}

- (void)disconnect {
  _modelObserverBridge.reset();
  _saveCardBottomSheetModel.reset();
}

- (void)onBottomSheetDismissedWithLinkClicked:(BOOL)linkClicked {
  // `_dismissing` would be `YES` if the bottomsheet was dismissed due `No
  // thanks` button in offer state or autodismissed in confirmation state or was
  // dismissed on link clicked.
  if (_dismissing) {
    return;
  }
  switch (_saveCardBottomSheetModel->save_card_state()) {
      // Bottomsheet is being dismissed before being accepted due to being
      // swiped away, tab changed or link clicked. Dismissal due to swipe or tab
      // change cannot be distinguished, both the actions are logged under the
      // same bottomsheet result `kSwiped`, reasoning that users are more likely
      // to dismiss the bottomsheet with a swipe than by switching tabs (e.g by
      // opening a link from another app).
    case autofill::SaveCardBottomSheetModel::SaveCardState::kOffered:
      _saveCardBottomSheetModel->OnCanceled();
      autofill::autofill_metrics::LogSaveCreditCardPromptResultIOS(
          linkClicked ? autofill::autofill_metrics::
                            SaveCreditCardPromptResultIOS::kLinkClicked
                      : autofill::autofill_metrics::
                            SaveCreditCardPromptResultIOS::kSwiped,
          _saveCardBottomSheetModel->save_card_delegate()->is_for_upload(),
          _saveCardBottomSheetModel->save_card_delegate()
              ->GetSaveCreditCardOptions(),
          autofill::autofill_metrics::SaveCreditCardPromptOverlayType::
              kBottomSheet);
      break;
    // Bottomsheet is being dismissed while showing loading state due to being
    // swiped away, tab changed or link clicked.
    case autofill::SaveCardBottomSheetModel::SaveCardState::kSaveInProgress:
      autofill::autofill_metrics::LogCreditCardUploadLoadingViewResultMetric(
          autofill::autofill_metrics::SaveCardPromptResult::kClosed);
      break;
    // Bottomsheet is being dismissed while showing success confirmation state
    // due to being swiped away, tab changed or link clicked.
    case autofill::SaveCardBottomSheetModel::SaveCardState::kSaved:
      autofill::autofill_metrics::
          LogCreditCardUploadConfirmationViewResultMetric(
              autofill::autofill_metrics::SaveCardPromptResult::kClosed,
              /*is_card_uploaded=*/true);
      break;
    // Bottomsheet would have already been dismissed on failure.
    case autofill::SaveCardBottomSheetModel::SaveCardState::kFailed:
      NOTREACHED();
  }
  _dismissing = YES;
}

- (BOOL)isDismissingForTesting {
  return _dismissing;
}

- (void)setConsumer:(id<SaveCardBottomSheetConsumer>)consumer {
  _consumer = consumer;

  if (!_consumer) {
    return;
  }

  [self.consumer
      setAboveTitleImage:NativeImage(
                             _saveCardBottomSheetModel->logo_icon_id())];
  [self.consumer
      setAboveTitleImageDescription:base::SysUTF16ToNSString(
                                        _saveCardBottomSheetModel
                                            ->logo_icon_description())];
  [self.consumer
      setTitle:base::SysUTF16ToNSString(_saveCardBottomSheetModel->title())];
  [self.consumer setSubtitle:base::SysUTF16ToNSString(
                                 _saveCardBottomSheetModel->subtitle())];
  [self.consumer
      setAcceptActionText:base::SysUTF16ToNSString(
                              _saveCardBottomSheetModel->accept_button_text())];
  [self.consumer
      setCancelActionText:base::SysUTF16ToNSString(
                              _saveCardBottomSheetModel->cancel_button_text())];

  [self.consumer setLegalMessages:[SaveCardMessageWithLinks
                                      convertFrom:_saveCardBottomSheetModel
                                                      ->legal_messages()]];

  [self.consumer
      setCardNameAndLastFourDigits:base::SysUTF16ToNSString(
                                       _saveCardBottomSheetModel
                                           ->card_name_last_four_digits())
                withCardExpiryDate:base::SysUTF16ToNSString(
                                       _saveCardBottomSheetModel
                                           ->card_expiry_date())
                       andCardIcon:NativeImage(_saveCardBottomSheetModel
                                                   ->issuer_icon_id())
         andCardAccessibilityLabel:base::SysUTF16ToNSString(
                                       _saveCardBottomSheetModel
                                           ->card_accessibility_description())];

  autofill::autofill_metrics::LogSaveCreditCardPromptResultIOS(
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kShown,
      _saveCardBottomSheetModel->save_card_delegate()->is_for_upload(),
      _saveCardBottomSheetModel->save_card_delegate()
          ->GetSaveCreditCardOptions(),
      autofill::autofill_metrics::SaveCreditCardPromptOverlayType::
          kBottomSheet);
}

#pragma mark - SaveCardBottomSheetMutator

- (void)didAccept {
  _saveCardBottomSheetModel->OnAccepted();
  autofill::autofill_metrics::LogSaveCreditCardPromptResultIOS(
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kAccepted,
      _saveCardBottomSheetModel->save_card_delegate()->is_for_upload(),
      _saveCardBottomSheetModel->save_card_delegate()
          ->GetSaveCreditCardOptions(),
      autofill::autofill_metrics::SaveCreditCardPromptOverlayType::
          kBottomSheet);

  [_consumer
      showLoadingStateWithAccessibilityLabel:
          base::SysUTF16ToNSString(
              _saveCardBottomSheetModel->loading_accessibility_description())];
  autofill::autofill_metrics::LogCreditCardUploadLoadingViewShownMetric(
      /*is_shown=*/true);
}

- (void)didCancel {
  _saveCardBottomSheetModel->OnCanceled();
  autofill::autofill_metrics::LogSaveCreditCardPromptResultIOS(
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kDenied,
      _saveCardBottomSheetModel->save_card_delegate()->is_for_upload(),
      _saveCardBottomSheetModel->save_card_delegate()
          ->GetSaveCreditCardOptions(),
      autofill::autofill_metrics::SaveCreditCardPromptOverlayType::
          kBottomSheet);
  _dismissing = YES;
  [_autofillCommandsHandler dismissSaveCardBottomSheet];
}

#pragma mark - SaveCardBottomSheetModel Observer

- (void)onCreditCardUploadCompleted:(BOOL)cardSaved {
  autofill::autofill_metrics::LogCreditCardUploadLoadingViewResultMetric(
      autofill::autofill_metrics::SaveCardPromptResult::kNotInteracted);

  if (cardSaved) {
    [_consumer showConfirmationState];
    autofill::autofill_metrics::LogCreditCardUploadConfirmationViewShownMetric(
        /*is_shown=*/true, /*is_card_uploaded=*/true);

    // Auto dismiss bottomsheet after showing successful save card confirmation.
    __weak __typeof(self) weakSelf = self;
    _autoDismissConfirmationTimer.Start(
        FROM_HERE,
        UIAccessibilityIsVoiceOverRunning()
            ? kConfirmationDismissDelayIfVoiceOverRunning
            : kConfirmationDismissDelay,
        base::BindOnce(
            [](__weak __typeof(self) weakSelf) {
              if (weakSelf) {
                [weakSelf dimissConfirmationStateOnTimeout];
              }
            },
            weakSelf));
  } else {
    _dismissing = YES;
    [_autofillCommandsHandler dismissSaveCardBottomSheet];
  }
}

#pragma mark - Private

- (void)dimissConfirmationStateOnTimeout {
  _autoDismissConfirmationTimer.Stop();
  autofill::autofill_metrics::LogCreditCardUploadConfirmationViewResultMetric(
      autofill::autofill_metrics::SaveCardPromptResult::kNotInteracted,
      /*is_card_uploaded=*/true);
  _dismissing = YES;
  [_autofillCommandsHandler dismissSaveCardBottomSheet];
}

@end
