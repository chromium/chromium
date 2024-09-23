// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_ENHANCED_SAFE_BROWSING_INLINE_PROMO_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_ENHANCED_SAFE_BROWSING_INLINE_PROMO_DELEGATE_H_

// Delegate handling events from the enhanced safe browsing inline promo.
@protocol EnhancedSafeBrowsingInlinePromoDelegate <NSObject>

// Dismisses the Enhanced Safe Browsing Inline Promo UI.
- (void)dismissEnhancedSafeBrowsingInlinePromo;

// Displays the Safe Browsing Settings Menu UI.
- (void)showSafeBrowsingSettingsMenu;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_ENHANCED_SAFE_BROWSING_INLINE_PROMO_DELEGATE_H_
