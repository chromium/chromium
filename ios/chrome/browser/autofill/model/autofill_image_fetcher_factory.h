// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_impl.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace autofill {

// Singleton that owns all AutofillImageFetcherImpls and associates them with
// profiles.
class AutofillImageFetcherFactory final
    : public TypedProfileKeyedServiceFactoryIOS<AutofillImageFetcherFactory,
                                                AutofillImageFetcherImpl> {
 public:
  explicit AutofillImageFetcherFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
