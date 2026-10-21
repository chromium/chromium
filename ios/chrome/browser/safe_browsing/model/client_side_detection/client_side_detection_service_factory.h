// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_

#include "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns `ClientSideDetectionService` objects, one for each
// active Profile.
class ClientSideDetectionServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ClientSideDetectionServiceFactory,
          safe_browsing::ClientSideDetectionService> {
 public:
  explicit ClientSideDetectionServiceFactory(PassKey key);

 private:
  // `ProfileKeyedServiceFactoryIOS` implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_
