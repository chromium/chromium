// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mutator.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"

// This mediator tracks SaveCardBottomSheetModel to update the view. It also
// receives user actions to be communicated to the model.
@interface SaveCardBottomSheetMediator : NSObject <SaveCardBottomSheetMutator>

// Consumer interface for updating the save card bottomsheet.
@property(nonatomic, weak) id<SaveCardBottomSheetConsumer> consumer;

// Initialize this mediator with the save card bottomsheet model.
- (instancetype)initWithUIModel:
                    (std::unique_ptr<autofill::SaveCardBottomSheetModel>)model
        autofillCommandsHandler:(id<AutofillCommands>)autofillCommandsHandler;

- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_MEDIATOR_H_
