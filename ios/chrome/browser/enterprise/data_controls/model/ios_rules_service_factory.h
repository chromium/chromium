// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_IOS_RULES_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_IOS_RULES_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace data_controls {

class IOSRulesService;

class IOSRulesServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  IOSRulesServiceFactory(const IOSRulesServiceFactory&) = delete;
  IOSRulesServiceFactory& operator=(const IOSRulesServiceFactory&) = delete;

  // Returns the instance of IOSRulesServiceFactory.
  static IOSRulesServiceFactory* GetInstance();

  // Returns the IOSRulesService for `profile`, creating it if it is not yet
  // created.
  static IOSRulesService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<IOSRulesServiceFactory>;

  IOSRulesServiceFactory();
  ~IOSRulesServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_IOS_RULES_SERVICE_FACTORY_H_
