// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_HELPER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_HELPER_FACTORY_H_

#include <memory>

#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#include "ios/components/security_interstitials/safe_browsing/safe_browsing_helper.h"

// Singleton that owns all SafeBrowsingHelpers and associates them with
// a profile.
class SafeBrowsingHelperFactory final
    : public TypedProfileKeyedServiceFactoryIOS<SafeBrowsingHelperFactory,
                                                SafeBrowsingHelper> {
 public:
  explicit SafeBrowsingHelperFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SAFE_BROWSING_HELPER_FACTORY_H_
