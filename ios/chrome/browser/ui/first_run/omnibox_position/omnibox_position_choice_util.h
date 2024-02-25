// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_UTIL_H_

#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"

class ChromeBrowserState;

/// Returns the address bar option that should be selected by default in the
/// promo.
ToolbarType DefaultSelectedOmniboxPosition();

/// Returns whether the fullscreen IPH promo of OmniboxPositionChoice should be
/// shown based on `browser_state`.
bool ShouldShowOmniboxPositionChoiceIPHPromo(ChromeBrowserState* browser_state);

/// Returns whether the FRE screen of OmniboxPositionChoice should be shown
/// based on `browser_state`.
bool ShouldShowOmniboxPositionChoiceInFRE(ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_OMNIBOX_POSITION_CHOICE_UTIL_H_
