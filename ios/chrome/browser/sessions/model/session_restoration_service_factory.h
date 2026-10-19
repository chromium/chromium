// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all SessionRestorationService and associates them with
// ProfileIOS.
class SessionRestorationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          SessionRestorationServiceFactory,
          SessionRestorationService> {
 public:
  explicit SessionRestorationServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_FACTORY_H_
