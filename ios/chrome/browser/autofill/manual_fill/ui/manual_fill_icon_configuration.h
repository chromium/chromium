// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ICON_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ICON_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@class FaviconAttributes;
@protocol ChromeContentConfiguration;

// Leading icon style for a manual fill summary row or detail card header.
enum class ManualFillIconStyle {
  // Symbol inside a rounded-square container.
  kSymbol,
  // Credit card network or custom card art icon.
  kCardIcon,
  // Site favicon (or `icon_image` symbol override when non-nil).
  kFavicon,
};

// Creates a leading `ChromeContentConfiguration` matching `icon_style`. When
// `icon_style` is `kFavicon`, a non-nil `icon_image` renders a centered symbol
// override instead of `favicon_attributes`.
NSObject<ChromeContentConfiguration>* CreateManualFillIconConfiguration(
    ManualFillIconStyle icon_style,
    UIImage* icon_image,
    FaviconAttributes* favicon_attributes);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_UI_MANUAL_FILL_ICON_CONFIGURATION_H_
