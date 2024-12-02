// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_LOG_ROUTER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_LOG_ROUTER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace autofill {

class LogRouter;

// A factory that associates autofill::LogRouter instances with
// profiles. This returns nullptr for off-the-record profiles.
class AutofillLogRouterFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static autofill::LogRouter* GetForProfile(ProfileIOS* profile);
  static AutofillLogRouterFactory* GetInstance();

 private:
  friend class base::NoDestructor<AutofillLogRouterFactory>;

  AutofillLogRouterFactory();
  ~AutofillLogRouterFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace autofill
#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_LOG_ROUTER_FACTORY_H_
