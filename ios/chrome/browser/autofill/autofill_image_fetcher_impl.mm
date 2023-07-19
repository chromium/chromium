// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_image_fetcher_impl.h"

#import <UIKit/UIKit.h>

#import "base/barrier_callback.h"
#import "base/strings/stringprintf.h"
#import "components/autofill/core/browser/payments/constants.h"
#import "components/image_fetcher/core/image_fetcher_impl.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  // TODO(crbug.com/1313616): Remove this once the server stops sending down
  // this static URL for some cards.
  if (card_art_url.spec() == kCapitalOneCardArtUrl) {
    return AutofillImageFetcher::ResolveCardArtURL(card_art_url);
  }

  // A FIFE image fetching param suffix is appended to the URL. The image
  // should be center cropped and of Size(32, 20). For better image quality
  // we fetch an image based on the screen pixel density, and scale it in
  // ResolveCardArtImage.
  const int width = 32 * screen_scale_;
  const int height = 20 * screen_scale_;
  return GURL(card_art_url.spec() +
              base::StringPrintf("=w%d-h%d-n", width, height));
}

gfx::Image AutofillImageFetcherImpl::ResolveCardArtImage(
    const GURL& card_art_url,
    const gfx::Image& card_art_image) {
  // Some Capital One cards have a static URL rather than 'proper' card art
  // metadata, and so cannot be fetched at different sizes. We defer handling
  // those images to the base class.
  //
  // TODO(crbug.com/1313616): Remove this once the server stops sending down
  // this static URL for some cards.
  if (card_art_url.spec() == kCapitalOneCardArtUrl) {
    return AutofillImageFetcher::ResolveCardArtImage(card_art_url,
                                                     card_art_image);
  }

  // The image has been fetched at Size(32, 20) * the screen pixel density,
  // however image_fetcher::IOSImageDecoderImpl creates a UIImage with scale=1
  // (irregardless of pixel density). Re-scale the UIImage so that it is 32x20
  // when rendered.
  UIImage* image = card_art_image.ToUIImage();
  image = [UIImage imageWithCGImage:[image CGImage]
                              scale:screen_scale_
                        orientation:image.imageOrientation];
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
