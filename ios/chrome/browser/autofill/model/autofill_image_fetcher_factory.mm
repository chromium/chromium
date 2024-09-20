// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill {
// static
AutofillImageFetcherImpl* AutofillImageFetcherFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
AutofillImageFetcherImpl* AutofillImageFetcherFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<AutofillImageFetcherImpl*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
AutofillImageFetcherFactory* AutofillImageFetcherFactory::GetInstance() {
  static base::NoDestructor<AutofillImageFetcherFactory> instance;
  return instance.get();
}

AutofillImageFetcherFactory::AutofillImageFetcherFactory()
    : BrowserStateKeyedServiceFactory(
          "AutofillImageFetcher",
          BrowserStateDependencyManager::GetInstance()) {}
AutofillImageFetcherFactory::~AutofillImageFetcherFactory() {}

std::unique_ptr<KeyedService>
AutofillImageFetcherFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<autofill::AutofillImageFetcherImpl>(
      context->GetSharedURLLoaderFactory());
}

}  // namespace autofill
