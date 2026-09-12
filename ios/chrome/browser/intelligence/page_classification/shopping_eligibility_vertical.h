// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_SHOPPING_ELIGIBILITY_VERTICAL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_SHOPPING_ELIGIBILITY_VERTICAL_H_

#import <optional>

#import "base/functional/callback_forward.h"

class GURL;
struct CategoryResult;

namespace commerce {
struct ProductInfo;
class ShoppingService;
}  // namespace commerce

// Default confidence score assigned when a page is confirmed as a product page.
// ShoppingService signals (IsShoppingPage + ProductInfo) provide high-precision
// verification of Product Detail Pages, corresponding to high confidence
// (0.85) in downstream classification and thresholding.
inline constexpr float kDefaultShoppingProductConfidence = 0.85f;

// Evaluates Shopping page eligibility based on commerce ShoppingService
// signals.
class ShoppingEligibilityVertical {
 public:
  ShoppingEligibilityVertical() = delete;

  // Callback type for asynchronous shopping eligibility evaluation.
  using Callback = base::OnceCallback<void(std::optional<CategoryResult>)>;

  // Evaluates Shopping page eligibility using `shopping_service` for `url`.
  // Checks for cached product info first, or asynchronously verifies via
  // ShoppingService and queries ProductInfo.
  static void Evaluate(commerce::ShoppingService* shopping_service,
                       const GURL& url,
                       Callback callback);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_SHOPPING_ELIGIBILITY_VERTICAL_H_
