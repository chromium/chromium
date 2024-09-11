// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace autofill {

class AutofillImageFetcherImpl;

// Singleton that owns all AutofillImageFetcherImpls and associates them with
// profiles.
class AutofillImageFetcherFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static AutofillImageFetcherImpl* GetForBrowserState(ProfileIOS* profile);

  static AutofillImageFetcherImpl* GetForProfile(ProfileIOS* profile);
  static AutofillImageFetcherFactory* GetInstance();

  AutofillImageFetcherFactory(const AutofillImageFetcherFactory&) = delete;
  AutofillImageFetcherFactory& operator=(const AutofillImageFetcherFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AutofillImageFetcherFactory>;

  AutofillImageFetcherFactory();
  ~AutofillImageFetcherFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
