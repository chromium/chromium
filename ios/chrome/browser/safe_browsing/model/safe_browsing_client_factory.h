// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class SafeBrowsingClient;

// Singleton that owns all SafeBrowsingClients and associates them with
// a profile.
class SafeBrowsingClientFactory : public BrowserStateKeyedServiceFactory {
 public:
  static SafeBrowsingClient* GetForProfile(ProfileIOS* profile);

  // Deprecated: use GetForProfile(...).
  static SafeBrowsingClient* GetForBrowserState(ProfileIOS* profile);
  static SafeBrowsingClientFactory* GetInstance();

  SafeBrowsingClientFactory(const SafeBrowsingClientFactory&) = delete;
  SafeBrowsingClientFactory& operator=(const SafeBrowsingClientFactory&) =
      delete;

 private:
  friend class base::NoDestructor<SafeBrowsingClientFactory>;

  SafeBrowsingClientFactory();
  ~SafeBrowsingClientFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_FACTORY_H_
