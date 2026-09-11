// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_UTIL_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_UTIL_H_

#import <UIKit/UIKit.h>

/// Returns whether the omnibox popout layout should be applied for the given
/// `traitCollection`.
BOOL ShouldApplyOmniboxPopoutLayout(UITraitCollection* traitCollection);

/// Returns whether the omnibox popout layout should be applied for the given
/// `environment`.
BOOL ShouldApplyOmniboxPopoutLayout(id<UITraitEnvironment> environment);

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_UTIL_H_
