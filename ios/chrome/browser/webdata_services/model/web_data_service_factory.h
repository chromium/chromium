// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBDATA_SERVICES_MODEL_WEB_DATA_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEBDATA_SERVICES_MODEL_WEB_DATA_SERVICE_FACTORY_H_

#import "base/memory/scoped_refptr.h"
#import "components/webdata_services/web_data_service_wrapper.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_access.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class KeywordWebDataService;
class TokenWebData;

namespace autofill {
class AutofillWebDataService;
}

namespace ios {
// Singleton that owns all WebDataServiceWrappers and associates them with
// ProfileIOS.
class WebDataServiceFactory final : public TypedProfileKeyedServiceFactoryIOS<
                                        WebDataServiceFactory,
                                        WebDataServiceWrapper,
                                        RejectImplicitAccessIfIncognito> {
 public:
  explicit WebDataServiceFactory(PassKey key);

  // Returns the AutofillWebDataService associated with `profile`.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForProfile(ProfileIOS* profile,
                               ServiceAccessType access_type);

  // Returns the account-scoped AutofillWebDataService associated with the
  // `profile`.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForAccount(ProfileIOS* profile,
                               ServiceAccessType access_type);

  // Returns the KeywordWebDataService associated with `profile`.
  static scoped_refptr<KeywordWebDataService> GetKeywordWebDataForProfile(
      ProfileIOS* profile,
      ServiceAccessType access_type);

  // Returns the TokenWebData associated with `profile`.
  static scoped_refptr<TokenWebData> GetTokenWebDataForProfile(
      ProfileIOS* profile,
      ServiceAccessType access_type);

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_WEBDATA_SERVICES_MODEL_WEB_DATA_SERVICE_FACTORY_H_
