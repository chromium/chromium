// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/shopping_eligibility_vertical.h"

#import "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "components/commerce/core/shopping_service.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "ios/chrome/browser/intelligence/page_classification/page_classification_service.h"
#import "url/gurl.h"

namespace {

bool IsValidProductInfo(
    const std::optional<commerce::ProductInfo>& product_info) {
  return product_info.has_value() &&
         (!product_info->title.empty() ||
          product_info->product_cluster_id.has_value());
}

void OnProductInfoResult(
    ShoppingEligibilityVertical::Callback callback,
    const GURL& url,
    const std::optional<const commerce::ProductInfo>& product_info) {
  if (product_info.has_value() && IsValidProductInfo(*product_info)) {
    std::move(callback).Run(CategoryResult{
        .category_type = page_content_annotations::CategoryType::kShopping,
        .score = kDefaultShoppingProductConfidence,
        .is_eligible = true,
    });
    return;
  }
  std::move(callback).Run(std::nullopt);
}

void OnIsShoppingPageResult(
    base::WeakPtr<commerce::ShoppingService> shopping_service,
    const GURL& url,
    ShoppingEligibilityVertical::Callback callback,
    const GURL& result_url,
    std::optional<bool> is_shopping_page) {
  if (!is_shopping_page.value_or(false) || !shopping_service) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Once confirmed as shopping-related, query detailed ProductInfo for the PDP.
  shopping_service->GetProductInfoForUrl(
      url, base::BindOnce(&OnProductInfoResult, std::move(callback)));
}

}  // namespace

// static
void ShoppingEligibilityVertical::Evaluate(
    commerce::ShoppingService* shopping_service,
    const GURL& url,
    Callback callback) {
  if (!shopping_service || !url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Fast path: check if product info is already available in cache.
  std::optional<commerce::ProductInfo> available_info =
      shopping_service->GetAvailableProductInfoForUrl(url);
  if (IsValidProductInfo(available_info)) {
    std::move(callback).Run(CategoryResult{
        .category_type = page_content_annotations::CategoryType::kShopping,
        .score = kDefaultShoppingProductConfidence,
        .is_eligible = true,
    });
    return;
  }

  // Asynchronous verification: check if URL is shopping-related via
  // OptimizationGuide before fetching ProductInfo.
  shopping_service->IsShoppingPage(
      url,
      base::BindOnce(&OnIsShoppingPageResult, shopping_service->AsWeakPtr(),
                     url, std::move(callback)));
}
