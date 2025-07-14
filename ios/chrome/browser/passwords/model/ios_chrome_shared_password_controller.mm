// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_shared_password_controller.h"

#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_suggestion_utils.h"

@implementation IOSChromeSharedPasswordController

#pragma mark - FormSuggestionProvider

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  SuggestionsReadyCompletion updatedCompletion = ^(
      NSArray<FormSuggestion*>* suggestions,
      id<FormSuggestionProvider> delegate) {
    NSMutableArray* suggestionsCopy =
        [NSMutableArray arrayWithCapacity:suggestions.count];

    // Set up backup password suggestions with the right icon and icon type.
    for (FormSuggestion* suggestion : suggestions) {
      if (suggestion.type == autofill::SuggestionType::kBackupPasswordEntry) {
        FormSuggestion* suggestionCopy = [FormSuggestion
                   suggestionWithValue:suggestion.value
                    displayDescription:suggestion.displayDescription
                                  icon:GetBackupPasswordSuggestionIcon()
                                  type:suggestion.type
                               payload:suggestion.payload
                        requiresReauth:suggestion.requiresReauth
            acceptanceA11yAnnouncement:suggestion.acceptanceA11yAnnouncement
                              metadata:suggestion.metadata];
        suggestionCopy.suggestionIconType = SuggestionIconType::kBackupPassword;
        [suggestionsCopy addObject:suggestionCopy];
      } else {
        [suggestionsCopy addObject:suggestion];
      }
    }

    completion(suggestionsCopy, delegate);
  };

  [super retrieveSuggestionsForForm:formQuery
                           webState:webState
                  completionHandler:updatedCompletion];
}

@end
