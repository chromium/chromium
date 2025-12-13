// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autofill_image_fetcher_impl.h"

#import <UIKit/UIKit.h>

#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"
#import "url/gurl.h"

namespace ios_web_view {

class WebViewAutofillImageFetcherImplTest : public PlatformTest {
 public:
  WebViewAutofillImageFetcherImplTest()
      : autofill_image_fetcher_impl_(nullptr) {
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  WebViewAutofillImageFetcherImpl* autofill_image_fetcher() {
    return &autofill_image_fetcher_impl_;
  }

  ~WebViewAutofillImageFetcherImplTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }

 private:
  WebViewAutofillImageFetcherImpl autofill_image_fetcher_impl_;
};

TEST_F(WebViewAutofillImageFetcherImplTest, ResolveCardArtURL) {
  // ResolveImageURL should append FIFE parameters, specifying an image that is
  // 40x24px scaled to the screen scale.
  autofill_image_fetcher()->SetScreenScaleForTesting(4);
  EXPECT_EQ(
      autofill_image_fetcher()->ResolveImageURL(
          GURL("https://www.example.com/fake_image1"),
          autofill::AutofillImageFetcherBase::ImageType::kCreditCardArtImage),
      GURL("https://www.example.com/fake_image1=w160-h96-s"));
}

TEST_F(WebViewAutofillImageFetcherImplTest, ResolveCardArtImage) {
  // On iOS, the underlying decoder for the image fetcher always decodes
  // into a scale=1 UIImage. ResolveImage for credit card images then re-scales
  // it to match the screen scale.
  //
  // For this test, we mimic this by creating a UIImage of scale 1 directly,
  // then making sure that ResolveImage re-scales it to the mocked screen scale
  // set on the WebViewAutofillImageFetcherImpl.
  UIImage* input_image =
      ui::ResourceBundle::GetSharedInstance()
          .GetNativeImageNamed(autofill::CreditCard::IconResourceId("visaCC"))
          .ToUIImage();
  input_image = [UIImage imageWithCGImage:[input_image CGImage]
                                    scale:1
                              orientation:input_image.imageOrientation];
  ASSERT_EQ(input_image.scale, 1);

  autofill_image_fetcher()->SetScreenScaleForTesting(7);
  gfx::Image card_art_image = autofill_image_fetcher()->ResolveImage(
      GURL("https://example.com/fake_image1"), gfx::Image(input_image),
      autofill::AutofillImageFetcherBase::ImageType::kCreditCardArtImage);
  EXPECT_EQ(card_art_image.ToUIImage().scale, 7);
}

// Server can return an empty image.
TEST_F(WebViewAutofillImageFetcherImplTest, ResolveCardArtImage_EmptyImage) {
  gfx::Image resolved_image = autofill_image_fetcher()->ResolveImage(
      GURL("https://example.com/fake_image1"), gfx::Image(),
      autofill::AutofillImageFetcherBase::ImageType::kCreditCardArtImage);
  EXPECT_TRUE(resolved_image.IsEmpty());
}

}  // namespace ios_web_view
