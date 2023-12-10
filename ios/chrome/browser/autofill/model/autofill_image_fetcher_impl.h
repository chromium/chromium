// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_IMPL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_IMPL_H_

#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"
#include "components/keyed_service/core/keyed_service.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace autofill {

// iOS implementation of AutofillImageFetcher, which provides a fetcher for
// custom credit card icons.
class AutofillImageFetcherImpl : public AutofillImageFetcher,
                                 public KeyedService {
 public:
  AutofillImageFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AutofillImageFetcherImpl() override;

  // AutofillImageFetcher:
  GURL ResolveCardArtURL(const GURL& card_art_url) override;
  gfx::Image ResolveCardArtImage(const GURL& card_art_url,
                                 const gfx::Image& card_art_image) override;
  image_fetcher::ImageFetcher* GetImageFetcher() override;
  base::WeakPtr<AutofillImageFetcher> GetWeakPtr() override;

  void SetScreenScaleForTesting(CGFloat scale);

 private:
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  // To make sure the fetched images are not blurry, we scale the fetched size
  // up by the pixel density or 'scale' of the screen. The scale is kept as a
  // member variable as it can be overridden in tests.
  CGFloat screen_scale_;

  base::WeakPtrFactory<AutofillImageFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_IMPL_H_
