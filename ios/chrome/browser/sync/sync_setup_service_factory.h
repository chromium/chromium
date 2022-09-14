// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SYNC_SETUP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_SYNC_SETUP_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class SyncSetupService;

// Singleton that owns all SyncSetupServices and associates them with
// BrowserStates.
class SyncSetupServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static SyncSetupService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static SyncSetupService* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);

  static SyncSetupServiceFactory* GetInstance();

  SyncSetupServiceFactory(const SyncSetupServiceFactory&) = delete;
  SyncSetupServiceFactory& operator=(const SyncSetupServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<SyncSetupServiceFactory>;

  SyncSetupServiceFactory();
  ~SyncSetupServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_SYNC_SETUP_SERVICE_FACTORY_H_
