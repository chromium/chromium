// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_UTILS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_UTILS_H_

#import <UIKit/UIKit.h>

#include "base/time/time.h"
#include "url/gurl.h"

@class NewTabPageColorPalette;
class TemplateURLService;

/// Block extracting a `UIColor` from a  `NewTabPageColorPalette`.
typedef UIColor* (^PaletteColorProvider)(NewTabPageColorPalette*);

// Whether the top of feed sync promo has met the criteria to be shown.
bool ShouldShowTopOfFeedSyncPromo();

// Retrieves the URL for the AIM web page. `query_start_time` is the time that
// the user clicked the submit button.
GURL GetUrlForAim(TemplateURLService* turl_service,
                  const base::Time& query_start_time);

/// Generates a `UIButtonConfigurationUpdateHandler` that will color its button
/// correctly for the current NTP theming status.
/// - `unthemedTintColor` is the button's foreground tint color when there is no
///   theme set (color or image)
/// - `paletteBackgroundColorProvider` provides the desired background color
///   of the button. It will be passed the current palette, or nil if there is
///   no theme set.
/// - `imageBlurEffectStyleOverride` if set, overrides the default background
///   blur style for when the NTP background is an image.
UIButtonConfigurationUpdateHandler CreateThemedButtonConfigurationUpdateHandler(
    UIColor* unthemedTintColor,
    PaletteColorProvider paletteBackgroundColorProvider,
    UIBlurEffectStyle imageBlurEffectStyleOverride =
        UIBlurEffectStyleSystemMaterial);

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_UTILS_H_
