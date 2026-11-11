// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_IOS_CHROME_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_IOS_CHROME_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_FACTORY_H_

#import <memory>

#import "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Creates instances of `RemoteActorCredentialSharingService` per profile.
class IOSChromeRemoteActorCredentialSharingServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeRemoteActorCredentialSharingServiceFactory,
          password_manager::RemoteActorCredentialSharingService> {
 public:
  explicit IOSChromeRemoteActorCredentialSharingServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_IOS_CHROME_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_FACTORY_H_
