// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_DEFAULT_ICONS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_DEFAULT_ICONS_H_

#import <Foundation/Foundation.h>

@class FormSuggestion;

namespace web {
class WebState;
}  // namespace web

// Returns a copy of `suggestions` where each suggestion without an icon gets
// the default icon of its type, if any. `web_state` is used to resolve Autofill
// AI entity icons and may be null.
NSArray<FormSuggestion*>* CopyFormSuggestionsWithDefaultIcons(
    NSArray<FormSuggestion*>* suggestions,
    web::WebState* web_state);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_FORM_SUGGESTION_DEFAULT_ICONS_H_
