// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_TOP_SITES_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_TOP_SITES_FACTORY_H_

#import "base/memory/ref_counted.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace history {
class TopSites;
}

namespace ios {
// TopSitesFactory is a singleton that associates history::TopSites instance to
// profiles.
class TopSitesFactory : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static scoped_refptr<history::TopSites> GetForBrowserState(
      ProfileIOS* profile);

  static scoped_refptr<history::TopSites> GetForProfile(ProfileIOS* profile);
  static TopSitesFactory* GetInstance();

  TopSitesFactory(const TopSitesFactory&) = delete;
  TopSitesFactory& operator=(const TopSitesFactory&) = delete;

 private:
  friend class base::NoDestructor<TopSitesFactory>;

  TopSitesFactory();
  ~TopSitesFactory() override;

  // RefcountedBrowserStateKeyedServiceFactory implementation.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_TOP_SITES_FACTORY_H_
