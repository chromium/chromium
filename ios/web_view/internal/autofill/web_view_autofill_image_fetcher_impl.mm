// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autofill_image_fetcher_impl.h"

#import <UIKit/UIKit.h>

#import "base/strings/strcat.h"
#import "base/strings/stringprintf.h"
#import "components/autofill/core/browser/payments/constants.h"
#import "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/image_fetcher/core/image_fetcher_impl.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/gfx/canvas.h"
#import "ui/gfx/geometry/rect_f.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/image/image_skia.h"
#import "ui/gfx/image/image_skia_operations.h"
#import "url/gurl.h"

namespace ios_web_view {

WebViewAutofillImageFetcherImpl::WebViewAutofillImageFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : image_fetcher_(image_fetcher::CreateIOSImageDecoder(),
                     url_loader_factory),
      screen_scale_([[UIScreen mainScreen] scale]) {}

WebViewAutofillImageFetcherImpl::~WebViewAutofillImageFetcherImpl() {}

GURL WebViewAutofillImageFetcherImpl::ResolveImageURL(
    const GURL& image_url,
    ImageType image_type) const {
  switch (image_type) {
    case ImageType::kCreditCardArtImage: {
      // Some Capital One cards have a static URL rather than 'proper' card art
      // metadata, and so cannot be fetched at different sizes. We defer
      // handling that URL to the base class.
      if (image_url.spec() == autofill::kCapitalOneCardArtUrl) {
        return image_url;
      }

      // A FIFE image fetching param suffix is appended to the URL. The image
      // should be center cropped and of Size(40, 24). For better image quality
      // we fetch an image based on the screen pixel density, and scale it in
      // ResolveCardArtImage.
      const int width = 40 * screen_scale_;
      const int height = 24 * screen_scale_;
      GURL::Replacements replacements;
      std::string path = base::StrCat(
          {image_url.path(), base::StringPrintf("=w%d-h%d-s", width, height)});
      replacements.SetPathStr(path);
      return image_url.ReplaceComponents(replacements);
    }
    case ImageType::kPixAccountImage:
      // Pay with Pix is only queried in Chrome on Android.
      NOTREACHED();
    case ImageType::kValuableImage:
      return image_url;
  }
}

image_fetcher::ImageFetcher*
WebViewAutofillImageFetcherImpl::GetImageFetcher() {
  return &image_fetcher_;
}

base::WeakPtr<autofill::AutofillImageFetcher>
WebViewAutofillImageFetcherImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebViewAutofillImageFetcherImpl::SetScreenScaleForTesting(
    CGFloat screen_scale) {
  screen_scale_ = screen_scale;
}

gfx::Image WebViewAutofillImageFetcherImpl::ResolveCardArtImage(
    const GURL& card_art_url,
    const gfx::Image& card_art_image) {
  // Some Capital One cards have a static URL rather than 'proper' card art
  // metadata, and so cannot be fetched at different sizes. We defer handling
  // those images to the base class.
  if (card_art_url.spec() == autofill::kCapitalOneCardArtUrl) {
    return card_art_image;
  }

  // The image has been fetched at Size(40, 24) * the screen pixel density,
  // however image_fetcher::IOSImageDecoderImpl creates a UIImage with scale=1
  // (irregardless of pixel density). We re-scale the UIImage so that it is
  // 40x24 when rendered, and also apply rounded corners and a border.
  UIImage* inputImage = card_art_image.ToUIImage();
  if (!inputImage) {
    return card_art_image;  // Return original if conversion fails.
  }
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = screen_scale_;
  CGRect drawingRect = CGRectMake(0, 0, 40, 24);

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:drawingRect.size
                                             format:format];
  UIImage* outputImage =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        // Define corner radius and border width.
        const CGFloat cornerRadius = 2.0;
        const CGFloat borderWidth = 1.0;
        // Create a pth for clipping with rounded corners.
        UIBezierPath* clipCornersPath =
            [UIBezierPath bezierPathWithRoundedRect:drawingRect
                                       cornerRadius:cornerRadius];
        [clipCornersPath addClip];
        [inputImage drawInRect:drawingRect
                     blendMode:kCGBlendModeNormal
                         alpha:1.0];

        // Draw a border inside the clipped area. The border is 1dp wide, with
        // rounded corners of 2dp, using Grey 300.
        [[UIColor systemGray3Color] setStroke];
        CGContextSetLineWidth(context.CGContext, borderWidth);
        CGRect borderRect =
            CGRectInset(drawingRect, borderWidth / 2, borderWidth / 2);
        UIBezierPath* borderPath =
            [UIBezierPath bezierPathWithRoundedRect:borderRect
                                       cornerRadius:cornerRadius];
        [borderPath stroke];
      }];

  return gfx::Image(outputImage);
}

gfx::Image WebViewAutofillImageFetcherImpl::ResolveValuableImage(
    const gfx::Image& valuable_image) {
  return valuable_image;
}

}  // namespace ios_web_view
