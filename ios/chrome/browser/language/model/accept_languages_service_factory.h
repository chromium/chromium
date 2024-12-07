// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_MODEL_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_MODEL_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace language {
class AcceptLanguagesService;
}

// AcceptLanguagesServiceFactory is a way to associate an
// AcceptLanguagesService instance to a Profile.
class AcceptLanguagesServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static language::AcceptLanguagesService* GetForProfile(ProfileIOS* profile);
  static AcceptLanguagesServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<AcceptLanguagesServiceFactory>;

  AcceptLanguagesServiceFactory();
  ~AcceptLanguagesServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_MODEL_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
