// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_impl.h"

#import <UIKit/UIKit.h>

#import "base/barrier_callback.h"
#import "base/strings/stringprintf.h"
#import "components/autofill/core/browser/payments/constants.h"
#import "components/image_fetcher/core/image_fetcher_impl.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

namespace autofill {

AutofillImageFetcherImpl::AutofillImageFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : image_fetcher_(std::make_unique<image_fetcher::ImageFetcherImpl>(
          image_fetcher::CreateIOSImageDecoder(),
          url_loader_factory)),
      screen_scale_([[UIScreen mainScreen] scale]) {}

AutofillImageFetcherImpl::~AutofillImageFetcherImpl() {}

GURL AutofillImageFetcherImpl::ResolveCardArtURL(const GURL& card_art_url) {
  // Some Capital One cards have a static URL rather than 'proper' card art
  // metadata, and so cannot be fetched at different sizes. We defer handling
  // that URL to the base class.
  //
  // TODO(crbug.com/40221039): Remove this once the server stops sending down
  // this static URL for some cards.
  if (card_art_url.spec() == kCapitalOneCardArtUrl) {
    return AutofillImageFetcher::ResolveCardArtURL(card_art_url);
  }

  // A FIFE image fetching param suffix is appended to the URL. The image
  // should be center cropped and of Size(40, 24). For better image quality
  // we fetch an image based on the screen pixel density, and scale it in
  // ResolveCardArtImage.
  const int width = 40 * screen_scale_;
  const int height = 24 * screen_scale_;
  return GURL(card_art_url.spec() +
              base::StringPrintf("=w%d-h%d-s", width, height));
}

gfx::Image AutofillImageFetcherImpl::ResolveCardArtImage(
    const GURL& card_art_url,
    const gfx::Image& card_art_image) {
  if (card_art_image.IsEmpty()) {
    return card_art_image;
  }

  // Some Capital One cards have a static URL rather than 'proper' card art
  // metadata, and so cannot be fetched at different sizes. We defer handling
  // those images to the base class.
  //
  // TODO(crbug.com/40221039): Remove this once the server stops sending down
  // this static URL for some cards.
  if (card_art_url.spec() == kCapitalOneCardArtUrl) {
    return AutofillImageFetcher::ResolveCardArtImage(card_art_url,
                                                     card_art_image);
  }

  // The image has been fetched at Size(40, 24) * the screen pixel density,
  // however image_fetcher::IOSImageDecoderImpl creates a UIImage with scale=1
  // (irregardless of pixel density). We re-scale the UIImage so that it is
  // 40x24 when rendered, and also apply rounded corners and a border.
  UIImage* image = card_art_image.ToUIImage();
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = screen_scale_;
  CGRect targetRect = CGRectMake(0, 0, 40, 24);

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:targetRect.size
                                             format:format];
  image = [renderer imageWithActions:^(
                        UIGraphicsImageRendererContext* context) {
    // Copy over the downloaded image, clipped to have 2dp rounded corners.
    UIBezierPath* clipCornersPath =
        [UIBezierPath bezierPathWithRoundedRect:targetRect cornerRadius:2.0];
    [clipCornersPath addClip];
    [image drawInRect:targetRect blendMode:kCGBlendModeNormal alpha:1.0];

    // Draw a 1dp inside stroke, with corner radius 2dp., Gray 300 @ 100%
    // opacity. This is intended to overlap the card icon image.
    [[UIColor colorNamed:kGrey300Color] setStroke];
    CGFloat lineWidth = 1.0;
    CGContextSetLineWidth(context.CGContext, lineWidth);
    CGRect insetTarget = CGRectInset(targetRect, lineWidth / 2, lineWidth / 2);
    UIBezierPath* path = [UIBezierPath bezierPathWithRoundedRect:insetTarget
                                                    cornerRadius:2.0];
    [path stroke];
  }];

  return gfx::Image(image);
}

image_fetcher::ImageFetcher* AutofillImageFetcherImpl::GetImageFetcher() {
  return image_fetcher_.get();
}
base::WeakPtr<AutofillImageFetcher> AutofillImageFetcherImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillImageFetcherImpl::SetScreenScaleForTesting(CGFloat screen_scale) {
  screen_scale_ = screen_scale;
}

}  // namespace autofill
