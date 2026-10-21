// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all SendTabToSelfSyncService and associates them with
// ProfileIOS.
class SendTabToSelfSyncServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          SendTabToSelfSyncServiceFactory,
          send_tab_to_self::SendTabToSelfSyncService> {
 public:
  explicit SendTabToSelfSyncServiceFactory(PassKey key);

  // Returns the default factory used to build SendTabToSelfSyncService. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
