// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_TAB_CONTEXT_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_TAB_CONTEXT_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "components/sync_tab_context/tab_context_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class TabContextSyncServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          TabContextSyncServiceFactory,
          sync_tab_context::TabContextSyncService> {
 public:
  explicit TabContextSyncServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_TAB_CONTEXT_SYNC_SERVICE_FACTORY_H_
