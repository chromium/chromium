// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base.h"

#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/password_suggestion_bottom_sheet_exit_reason.h"

@interface CredentialSuggestionBottomSheetMediatorBase ()

// List of suggestions to be shown in the bottom sheet.
@property(nonatomic, strong) NSArray<FormSuggestion*>* suggestions;

@end

@implementation CredentialSuggestionBottomSheetMediatorBase

- (void)disconnect {
}

- (BOOL)hasSuggestions {
  return [self.suggestions count] > 0;
}

- (void)didSelectSuggestion:(FormSuggestion*)formSuggestion
                    atIndex:(NSInteger)index
                 completion:(ProceduralBlock)completion {
}

- (void)logExitReason:(PasswordSuggestionBottomSheetExitReason)exitReason {
}

- (void)onDismissWithoutAnyCredentialAction {
}

#pragma mark - CredentialSuggestionBottomSheetDelegate

- (void)disableBottomSheet {
}

- (void)loadFaviconWithBlockHandler:
    (FaviconLoader::FaviconAttributesCompletionBlock)faviconLoadedBlock {
}

@end
