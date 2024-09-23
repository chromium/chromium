// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_MESSAGE_BRIDGE_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_MESSAGE_BRIDGE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class SharingMessageBridge;

// Singleton that owns all SharingMessageBridge and associates them with
// Profile.
class IOSSharingMessageBridgeFactory : public BrowserStateKeyedServiceFactory {
 public:
  static SharingMessageBridge* GetForProfile(ProfileIOS* profile);
  static SharingMessageBridge* GetForProfileIfExists(ProfileIOS* profile);
  static IOSSharingMessageBridgeFactory* GetInstance();

  IOSSharingMessageBridgeFactory(const IOSSharingMessageBridgeFactory&) =
      delete;
  IOSSharingMessageBridgeFactory& operator=(
      const IOSSharingMessageBridgeFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSSharingMessageBridgeFactory>;

  IOSSharingMessageBridgeFactory();
  ~IOSSharingMessageBridgeFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_MESSAGE_BRIDGE_FACTORY_H_
