// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SESSION_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SESSION_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class GURL;

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

// Singleton that owns all SessionSyncService and associates them with
// ProfileIOS.
class SessionSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static sync_sessions::SessionSyncService* GetForBrowserState(
      ProfileIOS* profile);

  static sync_sessions::SessionSyncService* GetForProfile(ProfileIOS* profile);
  static SessionSyncServiceFactory* GetInstance();

  SessionSyncServiceFactory(const SessionSyncServiceFactory&) = delete;
  SessionSyncServiceFactory& operator=(const SessionSyncServiceFactory&) =
      delete;

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
