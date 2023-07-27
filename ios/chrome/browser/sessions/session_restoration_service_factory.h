// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class SessionRestorationService;

// Singleton that owns all SessionRestorationService and associates them with
// ChromeBrowserState.
class SessionRestorationServiceFactory final
    : public BrowserStateKeyedServiceFactory {
 public:
  static SessionRestorationService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static SessionRestorationServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SessionRestorationServiceFactory>;

  SessionRestorationServiceFactory();
  ~SessionRestorationServiceFactory() final;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const final;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const final;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_FACTORY_H_
