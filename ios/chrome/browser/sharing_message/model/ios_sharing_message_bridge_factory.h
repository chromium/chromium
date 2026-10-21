// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_MESSAGE_BRIDGE_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_MESSAGE_BRIDGE_FACTORY_H_

#import "components/sharing_message/sharing_message_bridge.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all SharingMessageBridge and associates them with
// Profile.
class IOSSharingMessageBridgeFactory final
    : public TypedProfileKeyedServiceFactoryIOS<IOSSharingMessageBridgeFactory,
                                                SharingMessageBridge> {
 public:
  explicit IOSSharingMessageBridgeFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SHARING_MESSAGE_MODEL_IOS_SHARING_MESSAGE_BRIDGE_FACTORY_H_
