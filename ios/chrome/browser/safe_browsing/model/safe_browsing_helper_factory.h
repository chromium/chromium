// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_HELPER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_HELPER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class SafeBrowsingHelper;

// Singleton that owns all SafeBrowsingHelpers and associates them with
// a profile.
class SafeBrowsingHelperFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static SafeBrowsingHelperFactory* GetInstance();
  static SafeBrowsingHelper* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<SafeBrowsingHelperFactory>;

  SafeBrowsingHelperFactory();
  ~SafeBrowsingHelperFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_HELPER_FACTORY_H_
