// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_SUGGESTION_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_SUGGESTION_INTERNAL_H_

#import "components/autofill/core/common/unique_ids.h"
#import "ios/web_view/public/cwv_autofill_suggestion.h"

NS_ASSUME_NONNULL_BEGIN

@class FormSuggestion;

@interface CWVAutofillSuggestion ()

- (instancetype)initWithFormSuggestion:(FormSuggestion*)formSuggestion
                              formName:(NSString*)formName
                        formRendererID:(autofill::FormRendererId)formRendererID
                       fieldIdentifier:(NSString*)fieldIdentifier
                       fieldRendererID:
                           (autofill::FieldRendererId)fieldRendererID
                               frameID:(NSString*)frameID
                  isPasswordSuggestion:(BOOL)isPasswordSuggestion
    NS_DESIGNATED_INITIALIZER;

// The internal autofill form suggestion.
@property(nonatomic, readonly) FormSuggestion* formSuggestion;

@property(nonatomic, readonly) autofill::FormRendererId formRendererID;
@property(nonatomic, readonly) autofill::FieldRendererId fieldRendererID;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_SUGGESTION_INTERNAL_H_
