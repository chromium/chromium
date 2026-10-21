// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SESSION_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SESSION_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class GURL;

// Singleton that owns all SessionSyncService and associates them with
// ProfileIOS.
class SessionSyncServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          SessionSyncServiceFactory,
          sync_sessions::SessionSyncService> {
 public:
  explicit SessionSyncServiceFactory(PassKey key);

  static bool ShouldSyncURLForTesting(const GURL& url);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SESSION_SYNC_SERVICE_FACTORY_H_
