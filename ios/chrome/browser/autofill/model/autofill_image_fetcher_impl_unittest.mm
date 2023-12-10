// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_impl.h"

#import <UIKit/UIKit.h>

#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/gfx/image/image_unittest_util.h"
#import "url/gurl.h"

namespace autofill {

class AutofillImageFetcherImplTest : public PlatformTest {
 public:
  AutofillImageFetcherImplTest()
      : autofill_image_fetcher_impl_(
            std::make_unique<AutofillImageFetcherImpl>(nullptr)) {}

  AutofillImageFetcherImpl* autofill_image_fetcher() {
    return autofill_image_fetcher_impl_.get();
  }

 private:
  std::unique_ptr<AutofillImageFetcherImpl> autofill_image_fetcher_impl_;
};

TEST_F(AutofillImageFetcherImplTest, ResolveCardArtURL) {
  // ResolveCardArtURL should append FIFE parameters, specifying an image
  // that is 40x24px scaled to the screen scale.
  autofill_image_fetcher()->SetScreenScaleForTesting(4);
  EXPECT_EQ(autofill_image_fetcher()->ResolveCardArtURL(
                GURL("https://www.example.com/fake_image1")),
            GURL("https://www.example.com/fake_image1=w160-h96-s"));
}

TEST_F(AutofillImageFetcherImplTest, ResolveCardArtImage) {
  // On iOS, the underlying decoder for the image fetcher always decodes
  // into a scale=1 UIImage. ResolveCardArtImage then re-scales it to match
  // the screen scale.
  //
  // For this test, we mimic this by creating a UIImage of scale 1 directly,
  // then making sure that ResolveCardArtImage re-scales it to the mocked
  // screen scale set on the AutofillImageFetcherImpl.
  UIImage* input_image = gfx::test::CreatePlatformImage();
  input_image = [UIImage imageWithCGImage:[input_image CGImage]
                                    scale:1
                              orientation:input_image.imageOrientation];
  ASSERT_EQ(input_image.scale, 1);

  autofill_image_fetcher()->SetScreenScaleForTesting(7);
  gfx::Image card_art_image = autofill_image_fetcher()->ResolveCardArtImage(
      GURL("https://example.com/fake_image1"), gfx::Image(input_image));
  EXPECT_EQ(card_art_image.ToUIImage().scale, 7);
}

// Regression test for crbug.com/1484797, in which the server can return an
// empty image that caused AutofillImageFetcherImpl::ResolveCardArtImage to
// crash.
TEST_F(AutofillImageFetcherImplTest, ResolveCardArtImage_EmptyImage) {
  gfx::Image resolved_image = autofill_image_fetcher()->ResolveCardArtImage(
      GURL("https://example.com/fake_image1"), gfx::Image());
  EXPECT_TRUE(resolved_image.IsEmpty());
}

}  // namespace autofill
