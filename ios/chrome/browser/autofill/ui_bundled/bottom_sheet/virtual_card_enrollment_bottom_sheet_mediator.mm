// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_mediator.h"

#import <optional>

#import "base/feature_list.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#import "components/autofill/core/browser/payments/virtual_card_enroll_metrics_logger.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_data.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image_skia_util_ios.h"

namespace {

// Bridges the C++ Observer interface to the Objective-C mediator. Uses scoped
// observation to add and remove itself as the observer of the
// VirtualCardEnrollUiModel.
class UiModelObserverBridge
    : public autofill::VirtualCardEnrollUiModel::Observer {
 public:
  UiModelObserverBridge(
      autofill::VirtualCardEnrollUiModel* model,
      __weak VirtualCardEnrollmentBottomSheetMediator* mediator)
      : mediator_(mediator) {
    scoped_model_observation_.Observe(model);
  }
  ~UiModelObserverBridge() override = default;
  void OnEnrollmentProgressChanged(
      autofill::VirtualCardEnrollUiModel::EnrollmentProgress
          enrollment_progress) override {
    [mediator_ modelDidChangeEnrollmentProgress:enrollment_progress];
  }

 private:
  __weak VirtualCardEnrollmentBottomSheetMediator* mediator_;
  base::ScopedObservation<autofill::VirtualCardEnrollUiModel,
                          UiModelObserverBridge>
      scoped_model_observation_{this};
};

// The delay between showing the confirmation and dismissing the virtual card
// enrollment prompt.
const base::TimeDelta kConfirmationDismissDelay = base::Seconds(1.5);

}  // namespace

@interface VirtualCardEnrollmentBottomSheetMediator () {
  VirtualCardEnrollmentBottomSheetData* _bottomSheetData;

  // The following members will be destroyed in reverse order. So that
  // the C++ observer object is destroyed, before the model is destroyed.
  std::unique_ptr<autofill::VirtualCardEnrollUiModel> _model;
  std::unique_ptr<UiModelObserverBridge> _uiModelObserverBridge;

  std::optional<autofill::VirtualCardEnrollmentCallbacks> _callbacks;
  __weak id<BrowserCoordinatorCommands> _browserCoordinatorHandler;
}

@end

@implementation VirtualCardEnrollmentBottomSheetMediator

- (instancetype)initWithUIModel:
                    (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model
                      callbacks:
                          (autofill::VirtualCardEnrollmentCallbacks)callbacks
      browserCoordinatorHandler:
          (id<BrowserCoordinatorCommands>)browserCoordinatorHandler {
  self = [super init];
  if (self) {
    UIImage* icon = nil;
    bool cardArtAvailable = model->enrollment_fields().card_art_image;
    if (cardArtAvailable) {
      icon = UIImageFromImageSkia(*model->enrollment_fields().card_art_image);
    }
    autofill::VirtualCardEnrollMetricsLogger::OnCardArtAvailable(
        cardArtAvailable,
        model->enrollment_fields().virtual_card_enrollment_source);
    CreditCardData* creditCard = [[CreditCardData alloc]
        initWithCreditCard:model->enrollment_fields().credit_card
                      icon:icon];
    _bottomSheetData = [[VirtualCardEnrollmentBottomSheetData alloc]
             initWithCreditCard:creditCard
                          title:base::SysUTF16ToNSString(model->window_title())
             explanatoryMessage:base::SysUTF16ToNSString(
                                    model->explanatory_message())
               acceptActionText:base::SysUTF16ToNSString(
                                    model->accept_action_text())
               cancelActionText:base::SysUTF16ToNSString(
                                    model->cancel_action_text())
              learnMoreLinkText:base::SysUTF16ToNSString(
                                    model->learn_more_link_text())
        googleLegalMessageLines:[SaveCardMessageWithLinks
                                    convertFrom:model->enrollment_fields()
                                                    .google_legal_message]
        issuerLegalMessageLines:[SaveCardMessageWithLinks
                                    convertFrom:model->enrollment_fields()
                                                    .issuer_legal_message]];
    _model = std::move(model);
    _callbacks = std::move(callbacks);
    _browserCoordinatorHandler = browserCoordinatorHandler;
    if (base::FeatureList::IsEnabled(
            autofill::features::
                kAutofillEnableVcnEnrollLoadingAndConfirmation)) {
      _uiModelObserverBridge =
          std::make_unique<UiModelObserverBridge>(_model.get(), self);
    }
  }
  return self;
}

- (void)setConsumer:(id<VirtualCardEnrollmentBottomSheetConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setCardData:_bottomSheetData];
  autofill::VirtualCardEnrollMetricsLogger::OnShown(
      _model->enrollment_fields().virtual_card_enrollment_source,
      /*is_reshow=*/false);
}

#pragma mark VirtualCardEnrollmentBottomSheetMutator

- (void)didAccept {
  CHECK(_callbacks) << "Callbacks are not set. Callbacks should have been set "
                       "and called only once.";
  _callbacks->OnAccepted();
  _callbacks.reset();
  [self logResultMetric:autofill::VirtualCardEnrollmentBubbleResult::
                            VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED];
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation)) {
    [_consumer showLoadingState];
    autofill::LogVirtualCardEnrollmentLoadingViewShown(/*is_shown=*/true);
    return;
  }
  autofill::LogVirtualCardEnrollmentLoadingViewShown(/*is_shown=*/false);
  [_browserCoordinatorHandler dismissVirtualCardEnrollmentBottomSheet];
}

- (void)didCancel {
  CHECK(_callbacks) << "Callbacks are not set. Callbacks should have been set "
                       "and called only once.";
  _callbacks->OnDeclined();
  _callbacks.reset();
  [self logResultMetric:autofill::VirtualCardEnrollmentBubbleResult::
                            VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED];
  [_browserCoordinatorHandler dismissVirtualCardEnrollmentBottomSheet];
}

#pragma mark - VirtualCardEnrollUiModel Observer

- (void)modelDidChangeEnrollmentProgress:
    (autofill::VirtualCardEnrollUiModel::EnrollmentProgress)enrollmentProgress {
  switch (enrollmentProgress) {
    case autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kEnrolled: {
      [self.consumer showConfirmationState];
      autofill::LogVirtualCardEnrollmentConfirmationViewShown(
          /*is_shown=*/true,
          /*is_card_enrolled=*/true);
      __weak __typeof__(self) weakSelf = self;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(^{
            [weakSelf onFinishedConfirmationDelay];
          }),
          kConfirmationDismissDelay);
      break;
    }
    case autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kFailed:
      // Dismiss the virtual card enrollment bottom sheet. Failure messages are
      // expected to be initiated by the IOSChromePaymentsAutofillClient.
      [_browserCoordinatorHandler dismissVirtualCardEnrollmentBottomSheet];
      autofill::LogVirtualCardEnrollmentConfirmationViewShown(
          /*is_shown=*/true,
          /*is_card_enrolled=*/false);
      break;
    case autofill::VirtualCardEnrollUiModel::EnrollmentProgress::kOffered:
      // The enrollment progress is set by IOSChromePaymentsAutofillClient to
      // either kEnrolled or kFailed and cannot transition back to kOffered.
      NOTREACHED();
  }
}

#pragma mark - Private

// Logs the result metric attaching additional parameters from the model.
- (void)logResultMetric:(autofill::VirtualCardEnrollmentBubbleResult)result {
  autofill::VirtualCardEnrollMetricsLogger::OnDismissed(
      result, _model->enrollment_fields().virtual_card_enrollment_source,
      /*is_reshow=*/false, _model->enrollment_fields().previously_declined);
}

// Handles dismissal after confirmation was shown with a delay.
- (void)onFinishedConfirmationDelay {
  [_browserCoordinatorHandler dismissVirtualCardEnrollmentBottomSheet];
}

@end
