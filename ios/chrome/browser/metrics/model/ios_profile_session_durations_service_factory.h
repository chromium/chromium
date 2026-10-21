// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_

#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// A factory that owns all IOSProfileSessionDurationsService and associate
// the to ProfileIOS instances.
class IOSProfileSessionDurationsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSProfileSessionDurationsServiceFactory,
          IOSProfileSessionDurationsService> {
 public:
  explicit IOSProfileSessionDurationsServiceFactory(PassKey key);

 private:
  // BrowserContextKeyedServiceFactory:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_PROFILE_SESSION_DURATIONS_SERVICE_FACTORY_H_
