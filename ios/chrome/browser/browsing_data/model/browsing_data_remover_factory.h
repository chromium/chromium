// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_FACTORY_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class BrowsingDataRemover;
class ProfileIOS;

// Singleton that owns all BrowsingDataRemovers and associates them with
// ProfileIOS.
class BrowsingDataRemoverFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static BrowsingDataRemover* GetForProfile(ProfileIOS* profile);
  static BrowsingDataRemover* GetForProfileIfExists(ProfileIOS* profile);
  static BrowsingDataRemoverFactory* GetInstance();

 private:
  friend class base::NoDestructor<BrowsingDataRemoverFactory>;

  BrowsingDataRemoverFactory();
  ~BrowsingDataRemoverFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_FACTORY_H_
