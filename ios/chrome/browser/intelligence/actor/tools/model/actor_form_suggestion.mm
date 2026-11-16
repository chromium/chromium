// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"

#import <UIKit/UIKit.h>

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion_favicon_data_source.h"

namespace {

// Converts an `autofill::ActorFormFillingRequestedData` to its corresponding
// `autofill::SuggestionType`.
autofill::SuggestionType SuggestionTypeFromRequestedData(
    autofill::ActorFormFillingRequestedData requested_data) {
  switch (requested_data) {
    case autofill::ActorFormFillingRequestedData::kUnknown:
      NOTREACHED();
    case autofill::ActorFormFillingRequestedData::kAddress:
    case autofill::ActorFormFillingRequestedData::kShippingAddress:
    case autofill::ActorFormFillingRequestedData::kBillingAddress:
    case autofill::ActorFormFillingRequestedData::kHomeAddress:
    case autofill::ActorFormFillingRequestedData::kWorkAddress:
    case autofill::ActorFormFillingRequestedData::kContactInformation:
      return autofill::SuggestionType::kAddressEntry;
    case autofill::ActorFormFillingRequestedData::kCreditCard:
      return autofill::SuggestionType::kCreditCardEntry;
  }
}

}  // namespace

@implementation ActorFormSuggestion {
  // Favicon image loaded asynchronously via `faviconDataSource`.
  UIImage* _loadedIcon;
  // Latest handler awaiting an asynchronous `_loadedIcon` update.
  ProceduralBlock _iconUpdateHandler;
  // Whether favicon loading has already been initiated (or completed).
  BOOL _faviconLoadingInitiated;
}

- (instancetype)initWithCredential:(const actor_login::Credential&)credential {
  self = [super init];
  if (self) {
    _credential = credential;

    NSString* value = base::SysUTF16ToNSString(credential.username);
    NSString* displayDescription =
        base::SysUTF16ToNSString(credential.display_origin);

    UIImage* icon = nil;
    if (credential.type == actor_login::CredentialType::kFederated &&
        credential.federation_detail.has_value()) {
      icon = credential.federation_detail->brand_icon.ToUIImage();
    }

    autofill::SuggestionType suggestionType =
        (credential.type == actor_login::CredentialType::kFederated)
            ? autofill::SuggestionType::kIdentityCredential
            : autofill::SuggestionType::kPasswordEntry;

    _type = suggestionType;
    _formSuggestion =
        [FormSuggestion suggestionWithValue:value
                         displayDescription:displayDescription
                                       icon:icon
                                       type:suggestionType
                                    payload:autofill::Suggestion::Payload()
                             requiresReauth:YES];
  }
  return self;
}
- (instancetype)
    initWithActorSuggestion:(const autofill::ActorSuggestion&)suggestion
                   dataType:(autofill::ActorFormFillingRequestedData)dataType {
  self = [super init];
  if (self) {
    _autofillSuggestion = suggestion;
    autofill::SuggestionType suggestionType =
        SuggestionTypeFromRequestedData(dataType);
    _type = suggestionType;

    NSString* value = base::SysUTF8ToNSString(suggestion.title);
    NSString* display_description = base::SysUTF8ToNSString(suggestion.details);

    UIImage* icon = nil;
    SuggestionIconType suggestionIconType = SuggestionIconType::kNone;

    if (suggestion.icon.has_value()) {
      icon = suggestion.icon->ToUIImage();
    } else {
      switch (dataType) {
        case autofill::ActorFormFillingRequestedData::kHomeAddress:
          suggestionIconType = SuggestionIconType::kAccountHome;
          break;
        case autofill::ActorFormFillingRequestedData::kWorkAddress:
          suggestionIconType = SuggestionIconType::kAccountWork;
          break;
        case autofill::ActorFormFillingRequestedData::kContactInformation:
          suggestionIconType = SuggestionIconType::kContact;
          break;
        default:
          break;
      }
    }

    _formSuggestion =
        [FormSuggestion suggestionWithValue:value
                         displayDescription:display_description
                                       icon:icon
                                       type:suggestionType
                                    payload:autofill::Suggestion::Payload()
                             requiresReauth:NO];
    _formSuggestion.suggestionIconType = suggestionIconType;
  }
  return self;
}

- (UIImage*)icon {
  return _formSuggestion.icon ?: _loadedIcon;
}

- (void)loadIconWithUpdateHandler:(ProceduralBlock)updateHandler {
  if (self.icon) {
    return;
  }
  _iconUpdateHandler = [updateHandler copy];
  if (_faviconLoadingInitiated) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  _faviconLoadingInitiated = YES;
  if (![self.faviconDataSource actorFormSuggestion:self
                         loadFaviconWithCompletion:^(UIImage* icon) {
                           [weakSelf didFinishLoadingFavicon:icon];
                         }]) {
    _faviconLoadingInitiated = NO;
    _iconUpdateHandler = nil;
  }
}

#pragma mark - Private

// Updates `_loadedIcon` and notifies `_iconUpdateHandler` whenever a new
// non-nil `icon` arrives.
- (void)didFinishLoadingFavicon:(UIImage*)icon {
  if (!icon || _loadedIcon == icon) {
    return;
  }
  _loadedIcon = icon;
  if (_iconUpdateHandler) {
    _iconUpdateHandler();
  }
}

@end
