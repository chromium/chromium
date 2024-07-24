// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"

@implementation CWVAutofillSuggestion {
  BOOL _isPasswordSuggestion;
}

@synthesize formSuggestion = _formSuggestion;
@synthesize formName = _formName;
@synthesize fieldIdentifier = _fieldIdentifier;
@synthesize frameID = _frameID;
@synthesize suggestionType = _suggestionType;

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
    _suggestionType = CWVSuggestionType(static_cast<long>(formSuggestion.type));
  }
  return self;
}

#pragma mark - Public Methods

- (NSString*)value {
  return [_formSuggestion.value copy];
}

- (NSString*)displayDescription {
  return [_formSuggestion.displayDescription copy];
}

- (UIImage* __nullable)icon {
  return [_formSuggestion.icon copy];
}

- (BOOL)isPasswordSuggestion {
  return _isPasswordSuggestion;
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  return [NSString stringWithFormat:@"%@ value: %@, displayDescription: %@",
                                    super.debugDescription, self.value,
                                    self.displayDescription];
}

@end
