// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace send_tab_to_self {
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

// Singleton that owns all SendTabToSelfSyncService and associates them with
// ProfileIOS.
class SendTabToSelfSyncServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static send_tab_to_self::SendTabToSelfSyncService* GetForProfile(
      ProfileIOS* profile);
  static SendTabToSelfSyncServiceFactory* GetInstance();

  // Returns the default factory used to build SendTabToSelfSyncService. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<SendTabToSelfSyncServiceFactory>;

  SendTabToSelfSyncServiceFactory();
  ~SendTabToSelfSyncServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
