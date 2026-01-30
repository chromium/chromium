// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_DISPLAY_CONTEXT_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_DISPLAY_CONTEXT_H_

enum class PromoDisplayTime {
  kFreshNtp,
};

// Structure holding the context for displaying a promo.
struct PromoDisplayContext {
  // Whether current display will be on an NTP that hasn't scrolled.
  bool is_on_fresh_ntp = false;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMO_DISPLAY_CONTEXT_H_
