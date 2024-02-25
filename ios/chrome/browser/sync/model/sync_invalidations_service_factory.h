// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace syncer {
class SyncInvalidationsService;
}  // namespace syncer

class SyncInvalidationsServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  SyncInvalidationsServiceFactory(const SyncInvalidationsServiceFactory&) =
      delete;
  SyncInvalidationsServiceFactory& operator=(
      const SyncInvalidationsServiceFactory&) = delete;

  // Returned value may be nullptr in case if sync invalidations are disabled or
  // not supported.
  static syncer::SyncInvalidationsService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static SyncInvalidationsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SyncInvalidationsServiceFactory>;

  SyncInvalidationsServiceFactory();
  ~SyncInvalidationsServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
