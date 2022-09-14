// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class GURL;

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

// Singleton that owns all SessionSyncService and associates them with
// ChromeBrowserState.
class SessionSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static sync_sessions::SessionSyncService* GetForBrowserState(
      ChromeBrowserState* browser_state);

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

#endif  // IOS_CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_
