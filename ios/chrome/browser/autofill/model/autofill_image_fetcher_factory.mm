// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill {

// static
AutofillImageFetcherImpl* AutofillImageFetcherFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AutofillImageFetcherImpl>(
      profile, /*create=*/true);
}

// static
AutofillImageFetcherFactory* AutofillImageFetcherFactory::GetInstance() {
  static base::NoDestructor<AutofillImageFetcherFactory> instance;
  return instance.get();
}

AutofillImageFetcherFactory::AutofillImageFetcherFactory()
    : ProfileKeyedServiceFactoryIOS("AutofillImageFetcher") {}

AutofillImageFetcherFactory::~AutofillImageFetcherFactory() = default;

std::unique_ptr<KeyedService>
AutofillImageFetcherFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<autofill::AutofillImageFetcherImpl>(
      profile->GetSharedURLLoaderFactory());
}

}  // namespace autofill
