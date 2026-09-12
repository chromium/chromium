// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/shopping_eligibility_vertical.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "ios/chrome/browser/intelligence/page_classification/page_classification_service.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

class ShoppingEligibilityVerticalTest : public PlatformTest {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  commerce::MockShoppingService mock_shopping_service_;
};

// Tests that a null ShoppingService returns std::nullopt.
TEST_F(ShoppingEligibilityVerticalTest, TestNullShoppingService) {
  base::test::TestFuture<std::optional<CategoryResult>> future;
  ShoppingEligibilityVertical::Evaluate(/*shopping_service=*/nullptr,
                                        GURL("https://example.com/item"),
                                        future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

// Tests that an invalid URL scheme returns std::nullopt.
TEST_F(ShoppingEligibilityVerticalTest, TestInvalidUrlScheme) {
  base::test::TestFuture<std::optional<CategoryResult>> future;
  ShoppingEligibilityVertical::Evaluate(
      &mock_shopping_service_, GURL("chrome://version"), future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

// Tests fast path when product info is already available in cache.
TEST_F(ShoppingEligibilityVerticalTest, TestCachedProductInfoWithTitle) {
  GURL product_url("https://example.com/product/123");
  commerce::ProductInfo product_info;
  product_info.title = "Running Shoes";

  EXPECT_CALL(mock_shopping_service_,
              GetAvailableProductInfoForUrl(product_url))
      .WillOnce(testing::Return(product_info));

  base::test::TestFuture<std::optional<CategoryResult>> future;
  ShoppingEligibilityVertical::Evaluate(&mock_shopping_service_, product_url,
                                        future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_eligible);
  EXPECT_EQ(page_content_annotations::CategoryType::kShopping,
            result->category_type);
  EXPECT_FLOAT_EQ(kDefaultShoppingProductConfidence, result->score);
}

// Tests fast path when product info has a cluster ID without a title.
TEST_F(ShoppingEligibilityVerticalTest, TestCachedProductInfoWithClusterId) {
  GURL product_url("https://example.com/product/456");
  commerce::ProductInfo product_info;
  product_info.product_cluster_id = 123456789ULL;

  EXPECT_CALL(mock_shopping_service_,
              GetAvailableProductInfoForUrl(product_url))
      .WillOnce(testing::Return(product_info));

  base::test::TestFuture<std::optional<CategoryResult>> future;
  ShoppingEligibilityVertical::Evaluate(&mock_shopping_service_, product_url,
                                        future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_eligible);
  EXPECT_EQ(page_content_annotations::CategoryType::kShopping,
            result->category_type);
  EXPECT_FLOAT_EQ(kDefaultShoppingProductConfidence, result->score);
}

// Tests that non-shopping page returns std::nullopt.
TEST_F(ShoppingEligibilityVerticalTest, TestNotShoppingPage) {
  GURL non_shopping_url("https://example.com/article");

  EXPECT_CALL(mock_shopping_service_,
              GetAvailableProductInfoForUrl(non_shopping_url))
      .WillOnce(testing::Return(std::nullopt));
  EXPECT_CALL(mock_shopping_service_,
              IsShoppingPage(non_shopping_url, testing::_))
      .WillOnce([](const GURL& url, commerce::IsShoppingPageCallback callback) {
        std::move(callback).Run(url, false);
      });

  base::test::TestFuture<std::optional<CategoryResult>> future;
  ShoppingEligibilityVertical::Evaluate(&mock_shopping_service_,
                                        non_shopping_url, future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

// Tests two-stage verification when IsShoppingPage is true and ProductInfo has
// title.
TEST_F(ShoppingEligibilityVerticalTest, TestShoppingPageWithProductInfo) {
  GURL product_url("https://example.com/product/789");
  commerce::ProductInfo product_info;
  product_info.title = "Mechanical Keyboard";

  EXPECT_CALL(mock_shopping_service_,
              GetAvailableProductInfoForUrl(product_url))
      .WillOnce(testing::Return(std::nullopt));
  EXPECT_CALL(mock_shopping_service_, IsShoppingPage(product_url, testing::_))
      .WillOnce([](const GURL& url, commerce::IsShoppingPageCallback callback) {
        std::move(callback).Run(url, true);
      });
  EXPECT_CALL(mock_shopping_service_,
              GetProductInfoForUrl(product_url, testing::_))
      .WillOnce([product_info](const GURL& url,
                               commerce::ProductInfoCallback callback) {
        std::move(callback).Run(url, product_info);
      });

  base::test::TestFuture<std::optional<CategoryResult>> future;
  ShoppingEligibilityVertical::Evaluate(&mock_shopping_service_, product_url,
                                        future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_eligible);
  EXPECT_EQ(page_content_annotations::CategoryType::kShopping,
            result->category_type);
  EXPECT_FLOAT_EQ(kDefaultShoppingProductConfidence, result->score);
}

// Tests two-stage verification when IsShoppingPage is true but ProductInfo is
// empty.
TEST_F(ShoppingEligibilityVerticalTest, TestShoppingPageWithoutProductInfo) {
  GURL category_url("https://example.com/category/electronics");

  EXPECT_CALL(mock_shopping_service_,
              GetAvailableProductInfoForUrl(category_url))
      .WillOnce(testing::Return(std::nullopt));
  EXPECT_CALL(mock_shopping_service_, IsShoppingPage(category_url, testing::_))
      .WillOnce([](const GURL& url, commerce::IsShoppingPageCallback callback) {
        std::move(callback).Run(url, true);
      });
  EXPECT_CALL(mock_shopping_service_,
              GetProductInfoForUrl(category_url, testing::_))
      .WillOnce([](const GURL& url, commerce::ProductInfoCallback callback) {
        std::move(callback).Run(url, std::nullopt);
      });

  base::test::TestFuture<std::optional<CategoryResult>> future;
  ShoppingEligibilityVertical::Evaluate(&mock_shopping_service_, category_url,
                                        future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}
