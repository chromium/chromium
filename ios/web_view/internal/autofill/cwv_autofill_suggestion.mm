// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVAutofillSuggestion {
  BOOL _isPasswordSuggestion;
}

@synthesize formSuggestion = _formSuggestion;
@synthesize formName = _formName;
@synthesize fieldIdentifier = _fieldIdentifier;
@synthesize frameID = _frameID;

- (instancetype)initWithFormSuggestion:(FormSuggestion*)formSuggestion
                              formName:(NSString*)formName
                       fieldIdentifier:(NSString*)fieldIdentifier
                               frameID:(NSString*)frameID
                  isPasswordSuggestion:(BOOL)isPasswordSuggestion {
  self = [super init];
  if (self) {
    _formSuggestion = formSuggestion;
    _formName = [formName copy];
    _fieldIdentifier = [fieldIdentifier copy];
    _frameID = [frameID copy];
    _isPasswordSuggestion = isPasswordSuggestion;
  }
  return self;
}

#pragma mark - Public Methods

- (NSString*)value {
  return [_formSuggestion.value copy];
}

- (NSString*)displayDescription {
  if ([self isPasswordSuggestion]) {
    // An opaque password string used to hide the true length of the password.
    return @"••••••••";
  } else {
    return [_formSuggestion.displayDescription copy];
  }
}

- (UIImage* __nullable)icon {
  if ([_formSuggestion.icon length] == 0) {
    return nil;
  }
  int resourceID = autofill::CreditCard::IconResourceId(
      base::SysNSStringToUTF8(_formSuggestion.icon));
  return ui::ResourceBundle::GetSharedInstance()
      .GetNativeImageNamed(resourceID)
      .ToUIImage();
}

- (BOOL)isPasswordSuggestion {
  return _isPasswordSuggestion;
}

- (NSInteger)uniqueIdentifier {
  return _formSuggestion.identifier;
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  return [NSString stringWithFormat:@"%@ value: %@, displayDescription: %@",
                                    super.debugDescription, self.value,
                                    self.displayDescription];
}

@end
