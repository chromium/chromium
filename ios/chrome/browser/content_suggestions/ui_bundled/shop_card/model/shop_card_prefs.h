// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_MODEL_SHOP_CARD_PREFS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_MODEL_SHOP_CARD_PREFS_H_

class PrefRegistrySimple;

namespace shop_card_prefs {

extern const char kShopCardPriceDropUrlImpressions[];

// Registers the prefs associated with the Tips module.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace shop_card_prefs

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_MODEL_SHOP_CARD_PREFS_H_
