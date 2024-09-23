// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_MEDIATOR_H_

#import "components/autofill/core/browser/ui/payments/virtual_card_enroll_ui_model.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/virtual_card_enrollment_callbacks.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_mutator.h"

@protocol BrowserCoordinatorCommands;
@protocol VirtualCardEnrollmentBottomSheetConsumer;

// This mediator adapts the VirtualCardEnrollUiModel to the consumer interface.
@interface VirtualCardEnrollmentBottomSheetMediator
    : NSObject <VirtualCardEnrollmentBottomSheetMutator>

// The consumer interface for updating the virtual card enrollment display.
@property(nonatomic, weak) id<VirtualCardEnrollmentBottomSheetConsumer>
    consumer;

// Initialize this mediator with the ui model and callbacks from autofill.
- (instancetype)initWithUIModel:
                    (std::unique_ptr<autofill::VirtualCardEnrollUiModel>)model
                      callbacks:
                          (autofill::VirtualCardEnrollmentCallbacks)callbacks
      browserCoordinatorHandler:
          (id<BrowserCoordinatorCommands>)browserCoordinatorHandler;

#pragma mark - VirtualCardEnrollUiModel Observer methods

- (void)modelDidChangeEnrollmentProgress:
    (autofill::VirtualCardEnrollUiModel::EnrollmentProgress)enrollmentProgress;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_MEDIATOR_H_
