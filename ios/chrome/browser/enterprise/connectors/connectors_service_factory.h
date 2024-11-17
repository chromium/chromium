// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace enterprise_connectors {

class ConnectorsServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ConnectorsServiceFactory* GetInstance();
  static ConnectorsService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<ConnectorsServiceFactory>;

  ConnectorsServiceFactory();
  ~ConnectorsServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_FACTORY_H_
