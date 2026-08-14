// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIVERSAL_OPTOUT_MODEL_UNIVERSAL_OPTOUT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_UNIVERSAL_OPTOUT_MODEL_UNIVERSAL_OPTOUT_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace universal_optout {

class UniversalOptOutService;

// Factory for creating UniversalOptOutService on iOS.
class UniversalOptOutServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static UniversalOptOutService* GetForProfile(ProfileIOS* profile);
  static UniversalOptOutServiceFactory* GetInstance();

  UniversalOptOutServiceFactory(const UniversalOptOutServiceFactory&) = delete;
  UniversalOptOutServiceFactory& operator=(
      const UniversalOptOutServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<UniversalOptOutServiceFactory>;

  UniversalOptOutServiceFactory();
  ~UniversalOptOutServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace universal_optout

#endif  // IOS_CHROME_BROWSER_UNIVERSAL_OPTOUT_MODEL_UNIVERSAL_OPTOUT_SERVICE_FACTORY_H_
