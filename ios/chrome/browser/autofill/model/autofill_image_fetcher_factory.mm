// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_factory.h"

#import <utility>

#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill {

AutofillImageFetcherFactory::AutofillImageFetcherFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "AutofillImageFetcher") {}

std::unique_ptr<KeyedService>
AutofillImageFetcherFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<AutofillImageFetcherImpl>(
      profile->GetSharedURLLoaderFactory());
}

}  // namespace autofill
