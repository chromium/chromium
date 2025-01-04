// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SESSION_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SESSION_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class GURL;

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

// Singleton that owns all SessionSyncService and associates them with
// ProfileIOS.
class SessionSyncServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static sync_sessions::SessionSyncService* GetForProfile(ProfileIOS* profile);
  static SessionSyncServiceFactory* GetInstance();

  static bool ShouldSyncURLForTesting(const GURL& url);

 private:
  friend class base::NoDestructor<SessionSyncServiceFactory>;

  SessionSyncServiceFactory();
  ~SessionSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SESSION_SYNC_SERVICE_FACTORY_H_
