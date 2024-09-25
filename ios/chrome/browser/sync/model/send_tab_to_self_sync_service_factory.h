// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace send_tab_to_self {
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

// Singleton that owns all SendTabToSelfSyncService and associates them with
// ProfileIOS.
class SendTabToSelfSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static send_tab_to_self::SendTabToSelfSyncService* GetForProfile(
      ProfileIOS* profile);
  static SendTabToSelfSyncServiceFactory* GetInstance();

  // Returns the default factory used to build SendTabToSelfSyncService. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  SendTabToSelfSyncServiceFactory(const SendTabToSelfSyncServiceFactory&) =
      delete;
  SendTabToSelfSyncServiceFactory& operator=(
      const SendTabToSelfSyncServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<SendTabToSelfSyncServiceFactory>;

  SendTabToSelfSyncServiceFactory();
  ~SendTabToSelfSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
