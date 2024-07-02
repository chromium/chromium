// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_

#import "base/feature_list.h"

// Kill switch to revert the removal of lock icon. When this feature is
// enabled, the lock icon is shown in the omnibox for secure pages. When
// disabled, no icon is shown for secure pages.
BASE_DECLARE_FEATURE(kOmniboxLockIconEnabled);

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

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UI_FEATURES_H_
