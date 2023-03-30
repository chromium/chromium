// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_delegate.h"

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

class WebStateList;

@protocol PasswordSuggestionBottomSheetConsumer;

// This mediator fetches a list suggestions to display in the bottom sheet.
// It also manages filling the form when a suggestion is selected, as well
// as showing the keyboard if requested when the bottom sheet is dismissed.
@interface PasswordSuggestionBottomSheetMediator
    : NSObject <PasswordSuggestionBottomSheetDelegate>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                              params:
                                  (const autofill::FormActivityParams&)params;

// The bottom sheet suggestions consumer.
@property(nonatomic, strong) id<PasswordSuggestionBottomSheetConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
