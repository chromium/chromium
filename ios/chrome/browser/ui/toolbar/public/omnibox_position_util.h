// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_POSITION_UTIL_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_POSITION_UTIL_H_

#import "components/segmentation_platform/public/result.h"

namespace omnibox {

/// Whether the user is considered a new omnibox user.
bool IsNewUser();

/// Returns whether the user is a Safari Switcher.
/// Safari switcher will have the Omnibox at the bottom by default.
bool IsSafariSwitcher(
    const segmentation_platform::ClassificationResult& result);

}  // namespace omnibox

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_POSITION_UTIL_H_
