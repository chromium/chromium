// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios

namespace send_tab_to_self {

class SendTabToSelfClientServiceIOS;

// Singleton that owns all SendTabToSelfClientService and associates them with
// ios::ChromeBrowserState.
class SendTabToSelfClientServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static SendTabToSelfClientServiceFactory* GetInstance();
  static SendTabToSelfClientServiceIOS* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<SendTabToSelfClientServiceFactory>;

  SendTabToSelfClientServiceFactory();
  ~SendTabToSelfClientServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfClientServiceFactory);
};

}  // namespace send_tab_to_self

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_FACTORY_H_