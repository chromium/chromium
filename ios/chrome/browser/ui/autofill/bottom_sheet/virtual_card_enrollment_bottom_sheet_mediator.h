// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_MEDIATOR_H_

#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/virtual_card_enrollment_bottom_sheet_consumer.h"

// This mediator adapts the VirtualCardEnrollUiModel to the consumer interface.
@interface VirtualCardEnrollmentBottomSheetMediator : NSObject
@property(nonatomic) id<VirtualCardEnrollmentBottomSheetConsumer> consumer;

// Initialize this mediator with the ui model from autofill.
- (id)initWithUiModel:(autofill::VirtualCardEnrollUiModel)model;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_MEDIATOR_H_
