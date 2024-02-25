// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class FollowService;

// Singleton that owns all FollowServices and associates them with
// ChromeBrowserState.
class FollowServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static FollowService* GetForBrowserState(ChromeBrowserState* browser_state);

  static FollowServiceFactory* GetInstance();

  FollowServiceFactory(const FollowServiceFactory&) = delete;
  FollowServiceFactory& operator=(const FollowServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<FollowServiceFactory>;

  FollowServiceFactory();
  ~FollowServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_SERVICE_FACTORY_H_
