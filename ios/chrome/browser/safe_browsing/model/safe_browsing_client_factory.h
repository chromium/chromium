// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class SafeBrowsingClient;

// Singleton that owns all SafeBrowsingClients and associates them with
// a profile.
class SafeBrowsingClientFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static SafeBrowsingClient* GetForProfile(ProfileIOS* profile);
  static SafeBrowsingClientFactory* GetInstance();

 private:
  friend class base::NoDestructor<SafeBrowsingClientFactory>;

  SafeBrowsingClientFactory();
  ~SafeBrowsingClientFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_CLIENT_FACTORY_H_
