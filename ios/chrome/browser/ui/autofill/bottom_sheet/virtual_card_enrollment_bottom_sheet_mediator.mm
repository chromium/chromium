// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/virtual_card_enroll_metrics_logger.h"
#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_data.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image_skia_util_ios.h"

@interface VirtualCardEnrollmentBottomSheetMediator () {
  VirtualCardEnrollmentBottomSheetData* _bottomSheetData;
  autofill::VirtualCardEnrollUiModel _model;
}

@end

@implementation VirtualCardEnrollmentBottomSheetMediator

- (id)initWithUiModel:(autofill::VirtualCardEnrollUiModel)model {
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

@end
