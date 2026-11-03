// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/preset_credential_suggestion_bottom_sheet_mediator.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base+Subclassing.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/passwords/password_suggestion/ui/password_suggestion_utils.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

@implementation PresetCredentialSuggestionBottomSheetMediator {
  // Favicon loader.
  raw_ptr<FaviconLoader> _faviconLoader;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       faviconLoader:(FaviconLoader*)faviconLoader
                         suggestions:(NSArray<FormSuggestion*>*)suggestions {
  self = [super initWithWebStateList:webStateList
                        reauthModule:nil
                         requestInfo:std::nullopt];
  if (self) {
    _faviconLoader = faviconLoader;
    self.suggestions = suggestions;
  }
  return self;
}

#pragma mark - CredentialSuggestionBottomSheetMediatorBase

- (void)disconnect {
  _faviconLoader = nullptr;
  [super disconnect];
}

- (void)setConsumer:(id<CredentialSuggestionBottomSheetConsumer>)consumer {
  [super setConsumer:consumer];

  if (![self hasSuggestions]) {
    return;
  }

  [self.consumer
      setPrimaryActionString:l10n_util::GetNSString(
                                 IDS_IOS_CREDENTIAL_BOTTOM_SHEET_USE_PASSWORD)
       secondaryActionString:l10n_util::GetNSString(IDS_CANCEL)
        secondaryActionImage:nil];
}

#pragma mark - CredentialSuggestionBottomSheetDelegate

- (void)loadFaviconWithBlockHandler:
    (FaviconLoader::FaviconAttributesCompletionBlock)faviconLoadedBlock {
  if (_faviconLoader && !self.URL.is_empty()) {
    _faviconLoader->FaviconForPageUrl(
        self.URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
        /*fallback_to_google_server=*/NO, faviconLoadedBlock);
    return;
  }
  faviconLoadedBlock(GetDefaultGlobeFaviconAttributes(),
                     /*cached=*/true);
}

#pragma mark - Subclassing

- (void)selectSuggestion:(FormSuggestion*)suggestion
                 atIndex:(NSInteger)index
              completion:(ProceduralBlock)completion {
  if (completion) {
    completion();
  }
}

@end
