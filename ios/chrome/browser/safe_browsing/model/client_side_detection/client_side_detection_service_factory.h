// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class KeyedService;
class ProfileIOS;

namespace safe_browsing {
class ClientSideDetectionService;
}

// Singleton that owns `ClientSideDetectionService` objects, one for each
// active Profile.
class ClientSideDetectionServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the instance of `ClientSideDetectionService` associated with
  // this profile.
  static safe_browsing::ClientSideDetectionService* GetForProfile(
      ProfileIOS* profile);

  // Returns the singleton instance of this factory.
  static ClientSideDetectionServiceFactory* GetInstance();

  ClientSideDetectionServiceFactory(const ClientSideDetectionServiceFactory&) =
      delete;
  ClientSideDetectionServiceFactory& operator=(
      const ClientSideDetectionServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<ClientSideDetectionServiceFactory>;

  ClientSideDetectionServiceFactory();
  ~ClientSideDetectionServiceFactory() override;

  // `ProfileKeyedServiceFactoryIOS` implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_
