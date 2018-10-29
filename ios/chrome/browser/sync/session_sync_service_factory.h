// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class GURL;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// Singleton that owns all SessionSyncService and associates them with
// ios::ChromeBrowserState.
class SessionSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static sync_sessions::SessionSyncService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static SessionSyncServiceFactory* GetInstance();

  static bool ShouldSyncURLForTesting(const GURL& url);

 private:
  friend struct base::DefaultSingletonTraits<SessionSyncServiceFactory>;

  SessionSyncServiceFactory();
  ~SessionSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SessionSyncServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_SYNC_SESSION_SYNC_SERVICE_FACTORY_H_
