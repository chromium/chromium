// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_UI_FEATURES_H_
#define IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_UI_FEATURES_H_

#import "base/feature_list.h"

/// TODO(crbug.com/388820891): Update milestone after refactoring is complete.
const base::NotFatalUntil kOmniboxRefactoringNotFatalUntil =
    base::NotFatalUntil::M200;

// A tentative fix for crbug.com/361003475.
BASE_DECLARE_FEATURE(kBeginCursorAtPointTentativeFix);

// Returns whether rich autocompletion is enabled.
bool IsRichAutocompletionEnabled();

#endif  // IOS_CHROME_BROWSER_OMNIBOX_PUBLIC_OMNIBOX_UI_FEATURES_H_
