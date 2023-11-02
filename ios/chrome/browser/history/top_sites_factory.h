// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_TOP_SITES_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_TOP_SITES_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace history {
class TopSites;
}

namespace ios {
// TopSitesFactory is a singleton that associates history::TopSites instance to
// ChromeBrowserState.
class TopSitesFactory : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static scoped_refptr<history::TopSites> GetForBrowserState(
      ChromeBrowserState* browser_state);
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

#endif  // IOS_CHROME_BROWSER_HISTORY_TOP_SITES_FACTORY_H_
