// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LIVE_ACTIVITY_MODEL_IOS_CHROME_LIVE_ACTIVITY_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_LIVE_ACTIVITY_MODEL_IOS_CHROME_LIVE_ACTIVITY_MANAGER_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/live_activity/model/ios_chrome_live_activity_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all `IOSChromeLiveActivityManager` instances and
// associates them with regular `ProfileIOS` instances.
class IOSChromeLiveActivityManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeLiveActivityManagerFactory,
          IOSChromeLiveActivityManager> {
 public:
  explicit IOSChromeLiveActivityManagerFactory(PassKey key);

  // Returns the default factory used to build `IOSChromeLiveActivityManager`.
  // Can be registered with `AddTestingFactory()` to use real instances in unit
  // tests.
  static TestingFactory GetDefaultFactory();

 private:
  // `ProfileKeyedServiceFactoryIOS` implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS& profile) const override;
};

#endif  // IOS_CHROME_BROWSER_LIVE_ACTIVITY_MODEL_IOS_CHROME_LIVE_ACTIVITY_MANAGER_FACTORY_H_
