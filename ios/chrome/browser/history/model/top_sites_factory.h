// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_TOP_SITES_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_TOP_SITES_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "components/history/core/browser/top_sites.h"
#include "ios/chrome/browser/shared/model/profile/typed_refcounted_profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace ios {

// TopSitesFactory is a singleton that associates history::TopSites instance to
// profiles.
class TopSitesFactory
    : public TypedRefcountedProfileKeyedServiceFactoryIOS<TopSitesFactory,
                                                          history::TopSites> {
 public:
  TopSitesFactory(PassKey key);

 private:
  // RefcountedProfileKeyedServiceFactoryIOS implementation.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_TOP_SITES_FACTORY_H_
