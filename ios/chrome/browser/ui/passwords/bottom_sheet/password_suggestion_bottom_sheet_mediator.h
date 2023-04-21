// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_delegate.h"

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

namespace password_manager {
class SavedPasswordsPresenter;
}  // namespace password_manager

class FaviconLoader;
class PrefService;
class WebStateList;

@protocol PasswordSuggestionBottomSheetConsumer;

// This mediator fetches a list suggestions to display in the bottom sheet.
// It also manages filling the form when a suggestion is selected, as well
// as showing the keyboard if requested when the bottom sheet is dismissed.
@interface PasswordSuggestionBottomSheetMediator
    : NSObject <PasswordSuggestionBottomSheetDelegate>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       faviconLoader:(FaviconLoader*)faviconLoader
                         prefService:(PrefService*)prefService
                              params:(const autofill::FormActivityParams&)params
             savedPasswordsPresenter:
                 (raw_ptr<password_manager::SavedPasswordsPresenter>)
                     passwordPresenter;

// Disconnects the mediator.
- (void)disconnect;

// Whether the mediator has any suggestions for the user.
- (BOOL)hasSuggestions;

// The bottom sheet suggestions consumer.
@property(nonatomic, strong) id<PasswordSuggestionBottomSheetConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
