// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_mediator.h"

#import <optional>

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/virtual_card_enroll_metrics_logger.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_data.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image_skia_util_ios.h"

@interface VirtualCardEnrollmentBottomSheetMediator () {
  VirtualCardEnrollmentBottomSheetData* _bottomSheetData;
  autofill::VirtualCardEnrollUiModel _model;
  std::optional<autofill::VirtualCardEnrollmentCallbacks> _callbacks;
  __weak id<BrowserCoordinatorCommands> _browserCoordinatorCommands;
}

@end

@implementation VirtualCardEnrollmentBottomSheetMediator

- (id)initWithUiModel:(autofill::VirtualCardEnrollUiModel)model
                     callbacks:
                         (autofill::VirtualCardEnrollmentCallbacks)callbacks
    browserCoordinatorCommands:
        (id<BrowserCoordinatorCommands>)browserCoordinatorCommands {
  self = [super init];
  if (self) {
    UIImage* icon = nil;
    bool card_art_available = model.enrollment_fields.card_art_image;
    if (card_art_available) {
      icon = UIImageFromImageSkia(*model.enrollment_fields.card_art_image);
    }
    autofill::VirtualCardEnrollMetricsLogger::OnCardArtAvailable(
        card_art_available,
        model.enrollment_fields.virtual_card_enrollment_source);
    CreditCardData* creditCard = [[CreditCardData alloc]
        initWithCreditCard:model.enrollment_fields.credit_card
                      icon:icon];
    _bottomSheetData = [[VirtualCardEnrollmentBottomSheetData alloc]
             initWithCreditCard:creditCard
                          title:base::SysUTF16ToNSString(model.window_title)
             explanatoryMessage:base::SysUTF16ToNSString(
                                    model.explanatory_message)
               acceptActionText:base::SysUTF16ToNSString(
                                    model.accept_action_text)
               cancelActionText:base::SysUTF16ToNSString(
                                    model.cancel_action_text)
              learnMoreLinkText:base::SysUTF16ToNSString(
                                    model.learn_more_link_text)
        googleLegalMessageLines:[SaveCardMessageWithLinks
                                    convertFrom:model.enrollment_fields
                                                    .google_legal_message]
        issuerLegalMessageLines:[SaveCardMessageWithLinks
                                    convertFrom:model.enrollment_fields
                                                    .issuer_legal_message]];
    _model = std::move(model);
    _callbacks = std::move(callbacks);
    _browserCoordinatorCommands = browserCoordinatorCommands;
  }
  return self;
}

- (void)setConsumer:(id<VirtualCardEnrollmentBottomSheetConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setCardData:_bottomSheetData];
  autofill::VirtualCardEnrollMetricsLogger::OnShown(
      _model.enrollment_fields.virtual_card_enrollment_source,
      /*is_reshow=*/false);
}

#pragma mark VirtualCardEnrollmentBottomSheetMutator

- (void)didAccept {
  // TODO(crbug.com/339887700): Implement dismissing when enrollment has
  // completed and show a loading state meanwhile.
  CHECK(_callbacks) << "Callbacks_ are not set. Callbacks_ should have been "
                       "set and called only once.";
  _callbacks->OnAccepted();
  _callbacks.reset();
  [self logResultMetric:autofill::VirtualCardEnrollmentBubbleResult::
                            VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED];
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation)) {
    [_consumer showLoadingState];
    return;
  }
  [_browserCoordinatorCommands dismissVirtualCardEnrollmentBottomSheet];
}

- (void)didCancel {
  CHECK(_callbacks) << "Callbacks_ are not set. Callbacks_ should have been "
                       "set and called only once.";
  _callbacks->OnDeclined();
  _callbacks.reset();
  [self logResultMetric:autofill::VirtualCardEnrollmentBubbleResult::
                            VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED];
  [_browserCoordinatorCommands dismissVirtualCardEnrollmentBottomSheet];
}

#pragma mark - Private

// Logs the result metric attaching additional parameters from the model.
- (void)logResultMetric:(autofill::VirtualCardEnrollmentBubbleResult)result {
  autofill::VirtualCardEnrollMetricsLogger::OnDismissed(
      result, _model.enrollment_fields.virtual_card_enrollment_source,
      /*is_reshow=*/false, _model.enrollment_fields.previously_declined);
}

@end
