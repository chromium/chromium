// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace safe_browsing {
class ChromeEnterpriseRealTimeUrlLookupService;

// Singleton that owns ChromeEnterpriseRealTimeUrlLookupService objects, one for
// each active profile. It returns nullptr for Incognito profiles.
class ChromeEnterpriseRealTimeUrlLookupServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns null if `profile` is in Incognito mode.
  static ChromeEnterpriseRealTimeUrlLookupService* GetForProfile(
      ProfileIOS* profile);

  // Get the singleton instance.
  static ChromeEnterpriseRealTimeUrlLookupServiceFactory* GetInstance();

  ChromeEnterpriseRealTimeUrlLookupServiceFactory(
      const ChromeEnterpriseRealTimeUrlLookupServiceFactory&) = delete;
  ChromeEnterpriseRealTimeUrlLookupServiceFactory& operator=(
      const ChromeEnterpriseRealTimeUrlLookupServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ChromeEnterpriseRealTimeUrlLookupServiceFactory>;

  ChromeEnterpriseRealTimeUrlLookupServiceFactory();
  ~ChromeEnterpriseRealTimeUrlLookupServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_FACTORY_H_
