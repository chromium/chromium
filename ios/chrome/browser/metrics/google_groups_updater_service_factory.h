// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_GOOGLE_GROUPS_UPDATER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_GOOGLE_GROUPS_UPDATER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class GoogleGroupsUpdaterService;

class GoogleGroupsUpdaterServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for `browser_state`.
  static GoogleGroupsUpdaterService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static GoogleGroupsUpdaterServiceFactory* GetInstance();

  GoogleGroupsUpdaterServiceFactory(const GoogleGroupsUpdaterServiceFactory&) =
      delete;
  GoogleGroupsUpdaterServiceFactory& operator=(
      const GoogleGroupsUpdaterServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<GoogleGroupsUpdaterServiceFactory>;

  GoogleGroupsUpdaterServiceFactory();
  ~GoogleGroupsUpdaterServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  bool ServiceIsCreatedWithBrowserState() const override;

  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_GOOGLE_GROUPS_UPDATER_SERVICE_FACTORY_H_
