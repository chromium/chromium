// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_FACTORY_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class BrowsingDataRemover;

// Singleton that owns all BrowsingDataRemovers and associates them with
// ProfileIOS.
class BrowsingDataRemoverFactory : public BrowserStateKeyedServiceFactory {
 public:
  static BrowsingDataRemover* GetForProfile(ProfileIOS* profile);
  static BrowsingDataRemover* GetForProfileIfExists(ProfileIOS* profile);
  static BrowsingDataRemoverFactory* GetInstance();

  BrowsingDataRemoverFactory(const BrowsingDataRemoverFactory&) = delete;
  BrowsingDataRemoverFactory& operator=(const BrowsingDataRemoverFactory&) =
      delete;

 private:
  friend class base::NoDestructor<BrowsingDataRemoverFactory>;

  BrowsingDataRemoverFactory();
  ~BrowsingDataRemoverFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_FACTORY_H_
