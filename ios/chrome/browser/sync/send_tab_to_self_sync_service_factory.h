// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace send_tab_to_self {
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// Singleton that owns all SendTabToSelfSyncService and associates them with
// ios::ChromeBrowserState.
class SendTabToSelfSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static send_tab_to_self::SendTabToSelfSyncService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static SendTabToSelfSyncServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SendTabToSelfSyncServiceFactory>;

  SendTabToSelfSyncServiceFactory();
  ~SendTabToSelfSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfSyncServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_SYNC_SEND_TAB_TO_SELF_SYNC_SERVICE_FACTORY_H_
