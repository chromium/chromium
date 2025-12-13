// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {

class AutofillImageFetcherImpl;

// Singleton that owns all AutofillImageFetcherImpls and associates them with
// profiles.
class AutofillImageFetcherFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static AutofillImageFetcherImpl* GetForProfile(ProfileIOS* profile);
  static AutofillImageFetcherFactory* GetInstance();

 private:
  friend class base::NoDestructor<AutofillImageFetcherFactory>;

  AutofillImageFetcherFactory();
  ~AutofillImageFetcherFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
