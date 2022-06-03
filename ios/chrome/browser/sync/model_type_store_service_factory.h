// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_TYPE_STORE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_TYPE_STORE_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace syncer {
class ModelTypeStoreService;
}  // namespace syncer

// Singleton that owns all ModelTypeStoreService and associates them with
// ChromeBrowserState.
class ModelTypeStoreServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::ModelTypeStoreService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static ModelTypeStoreServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ModelTypeStoreServiceFactory>;

  ModelTypeStoreServiceFactory();
  ~ModelTypeStoreServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_TYPE_STORE_SERVICE_FACTORY_H_
