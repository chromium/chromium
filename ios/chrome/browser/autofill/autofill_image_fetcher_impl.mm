// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_image_fetcher_impl.h"

#import "base/barrier_callback.h"
#import "components/image_fetcher/core/image_fetcher_impl.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

AutofillImageFetcherImpl::AutofillImageFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : image_fetcher_(std::make_unique<image_fetcher::ImageFetcherImpl>(
          image_fetcher::CreateIOSImageDecoder(),
          url_loader_factory)) {}

AutofillImageFetcherImpl::~AutofillImageFetcherImpl() {}

image_fetcher::ImageFetcher* AutofillImageFetcherImpl::GetImageFetcher() {
  return image_fetcher_.get();
}
base::WeakPtr<AutofillImageFetcher> AutofillImageFetcherImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
