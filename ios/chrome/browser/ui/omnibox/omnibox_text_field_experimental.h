// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_EXPERIMENTAL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_EXPERIMENTAL_H_

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"

// A textfield with inline autocomplete and pre-edit states.
// Pre-edit: the state when the text is "selected" and will erase upon typing.
// Unlike a normal iOS selection, no selection handles are displayed. Inline
// autocomplete: there's an optional autocomplete text following the caret.
@interface OmniboxTextFieldExperimental : OmniboxTextFieldIOS

// Returns self.text without the autocomplete, if it's available.
- (NSString*)userText;

@property(nonatomic, assign, getter=isPreEditing) BOOL preEditing;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_EXPERIMENTAL_H_
