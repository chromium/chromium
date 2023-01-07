// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class KeyedService;

namespace safe_browsing {
class TailoredSecurityService;
}

namespace web {
class BrowserState;
}

// Singleton that owns TailoredSecurityService objects, one for each active
// ChromeBrowserState. It returns nullptr for Incognito browser states.
class TailoredSecurityServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of TailoredSecurityService associated with this
  // browser state, creating one if none exists and the given browser state is
  // not in Incognito mode.
  static safe_browsing::TailoredSecurityService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  // Returns the singleton instance of TailoredSecurityServiceFactory.
  static TailoredSecurityServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<TailoredSecurityServiceFactory>;

  TailoredSecurityServiceFactory();
  ~TailoredSecurityServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_
