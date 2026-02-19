// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/password_suggestion_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_consumer.h"
#import "url/gurl.h"
#import "url/origin.h"

@interface CredentialSuggestionBottomSheetMediatorBase ()

// Origin to fetch credentials for.
@property(nonatomic, assign) GURL URL;

// Domain of the URL to fetch credentials for.
@property(nonatomic, strong) NSString* domain;

// List of suggestions to be shown in the bottom sheet.
@property(nonatomic, strong) NSArray<FormSuggestion*>* suggestions;

@end

@implementation CredentialSuggestionBottomSheetMediatorBase

- (instancetype)initWithURL:(const GURL&)URL {
  self = [super init];
  if (self) {
    _URL = URL;

    _domain = @"";
    if (!_URL.is_empty()) {
      url::Origin origin = url::Origin::Create(_URL);
      _domain =
          base::SysUTF8ToNSString(password_manager::GetShownOrigin(origin));
    }
  }
  return self;
}

- (void)setConsumer:(id<CredentialSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;

  // The bottom sheet isn't presented when there are no suggestions to show, so
  // there's no need to update the consumer.
  if (![self hasSuggestions]) {
    return;
  }

  [_consumer setSuggestions:self.suggestions andDomain:self.domain];
}

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
