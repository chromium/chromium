// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_LOCAL_UPDATE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_LOCAL_UPDATE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace tab_groups {

class TabGroupLocalUpdateService;

// Factory for the Tab Group local updates service.
class TabGroupLocalUpdateServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the TabGroupLocalUpdateService for this `browser_state`.
  static TabGroupLocalUpdateService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static TabGroupLocalUpdateServiceFactory* GetInstance();

  TabGroupLocalUpdateServiceFactory(const TabGroupLocalUpdateServiceFactory&) =
      delete;
  TabGroupLocalUpdateServiceFactory& operator=(
      const TabGroupLocalUpdateServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<TabGroupLocalUpdateServiceFactory>;

  TabGroupLocalUpdateServiceFactory();
  ~TabGroupLocalUpdateServiceFactory() override = default;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_LOCAL_UPDATE_SERVICE_FACTORY_H_
