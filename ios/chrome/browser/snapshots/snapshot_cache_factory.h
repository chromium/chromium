// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_FACTORY_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

@class SnapshotCache;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ios {
class ChromeBrowserState;
}

// Singleton that owns all SnapshotCaches and associates them with
// ios::ChromeBrowserState.
class SnapshotCacheFactory : public BrowserStateKeyedServiceFactory {
 public:
  static SnapshotCache* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static SnapshotCacheFactory* GetInstance();

  // Returns the default factory used to build SnapshotCaches. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend struct base::DefaultSingletonTraits<SnapshotCacheFactory>;

  SnapshotCacheFactory();
  ~SnapshotCacheFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(SnapshotCacheFactory);
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_FACTORY_H_
