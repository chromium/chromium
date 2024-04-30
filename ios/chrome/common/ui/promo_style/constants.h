// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_PROMO_STYLE_CONSTANTS_H_
#define IOS_CHROME_COMMON_UI_PROMO_STYLE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// A11y Identifier for header view background image.
extern NSString* const kPromoStyleHeaderViewBackgroundAccessibilityIdentifier;

// A11y Identifier for title label.
extern NSString* const kPromoStyleTitleAccessibilityIdentifier;

// A11y Identifier for subtitle label.
extern NSString* const kPromoStyleSubtitleAccessibilityIdentifier;

// A11y Identifier for the read more action button.
extern NSString* const kPromoStyleReadMoreActionAccessibilityIdentifier;

// A11y Identifier for the primary action button.
extern NSString* const kPromoStylePrimaryActionAccessibilityIdentifier;

// A11y Identifier for the secondary action button.
extern NSString* const kPromoStyleSecondaryActionAccessibilityIdentifier;

// A11y Identifier for the tertiary action button.
extern NSString* const kPromoStyleTertiaryActionAccessibilityIdentifier;

// A11y Identifier for the learn more button.
extern NSString* const kPromoStyleLearnMoreActionAccessibilityIdentifier;

// A11y Identifier for the disclaimer.
extern NSString* const kPromoStyleDisclaimerViewAccessibilityIdentifier;

// A11y Identifier for the scroll view that contains all the labels and buttons.
extern NSString* const kPromoStyleScrollViewAccessibilityIdentifier;

// Default margin for the promo view controller.
extern const CGFloat kPromoStyleDefaultMargin;
// Margin below action buttons in the FRE, when the screen has a safe area.
extern const CGFloat kActionsBottomMarginWithSafeArea;
// Margin below action buttons in the FRE, when the screen has no a safe area.
extern const CGFloat kActionsBottomMarginWithoutSafeArea;

#endif  // IOS_CHROME_COMMON_UI_PROMO_STYLE_CONSTANTS_H_
