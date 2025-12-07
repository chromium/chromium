// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_IMAGE_FETCHER_IMPL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_IMAGE_FETCHER_IMPL_H_

#include <Foundation/Foundation.h>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/keyed_service/core/keyed_service.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace autofill {
class AutofillImageFetcher;
}  // namespace autofill

namespace ios_web_view {

// ChromeWebView implementation of AutofillImageFetcher for iGA clients.
class WebViewAutofillImageFetcherImpl : public autofill::AutofillImageFetcher,
                                        public KeyedService {
 public:
  explicit WebViewAutofillImageFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~WebViewAutofillImageFetcherImpl() override;

  // autofill::AutofillImageFetcher:
  GURL ResolveImageURL(const GURL& image_url,
                       ImageType image_type) const override;
  image_fetcher::ImageFetcher* GetImageFetcher() override;
  base::WeakPtr<autofill::AutofillImageFetcher> GetWeakPtr() override;

  void SetScreenScaleForTesting(CGFloat scale);

 protected:
  // AutofillImageFetcher:
  gfx::Image ResolveCardArtImage(const GURL& card_art_url,
                                 const gfx::Image& card_art_image) override;
  gfx::Image ResolveValuableImage(const gfx::Image& valuable_image) override;

 private:
  // The image fetcher attached.
  image_fetcher::ImageFetcherImpl image_fetcher_;

  // To make sure the fetched images are not blurry, we scale the fetched size
  // up by the pixel density or 'scale' of the screen. The scale is kept as a
  // member variable as it can be overridden in tests.
  CGFloat screen_scale_;

  base::WeakPtrFactory<WebViewAutofillImageFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_IMAGE_FETCHER_IMPL_H_
