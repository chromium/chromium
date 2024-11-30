// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_UI_FEATURES_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_UI_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable actions in suggest.
BASE_DECLARE_FEATURE(kOmniboxActionsInSuggest);

// Type of rich autocompletion implementation.
enum class RichAutocompletionImplementation {
  // kRichAutocompletionParamLabel.
  kLabel,
  // kRichAutocompletionParamTextField.
  kTextField,
  // kRichAutocompletionParamNoAdditionalText.
  kNoAdditionalText,
  // Any implementation type.
  kAny,
};

// Returns whether kRichAutocompletion feature is enabled.
bool IsRichAutocompletionEnabled();

// Returns whether rich autocompletion implementation of `type` is enabled.
bool IsRichAutocompletionEnabled(RichAutocompletionImplementation type);

// Feature param for kRichAutocompletion.
extern const char kRichAutocompletionParam[];
// Rich autocompletion is shown in a UILabel after the text field.
extern const char kRichAutocompletionParamLabel[];
// Rich autocompletion is shown inside of the text field.
extern const char kRichAutocompletionParamTextField[];
// Rich autocompletion with no additional text.
extern const char kRichAutocompletionParamNoAdditionalText[];

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_OMNIBOX_UI_FEATURES_H_
