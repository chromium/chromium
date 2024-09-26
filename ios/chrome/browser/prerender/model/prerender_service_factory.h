// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class PrerenderService;

// Singleton that creates the PrerenderService and associates that service with
// profile.
class PrerenderServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static PrerenderService* GetForProfile(ProfileIOS* profile);
  static PrerenderServiceFactory* GetInstance();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

  PrerenderServiceFactory(const PrerenderServiceFactory&) = delete;
  PrerenderServiceFactory& operator=(const PrerenderServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<PrerenderServiceFactory>;

  PrerenderServiceFactory();
  ~PrerenderServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_SERVICE_FACTORY_H_
