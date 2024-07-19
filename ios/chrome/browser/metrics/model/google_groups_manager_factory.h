// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class GoogleGroupsManager;

class GoogleGroupsManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for `browser_state`.
  static GoogleGroupsManager* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static GoogleGroupsManagerFactory* GetInstance();

  GoogleGroupsManagerFactory(const GoogleGroupsManagerFactory&) =
      delete;
  GoogleGroupsManagerFactory& operator=(
      const GoogleGroupsManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<GoogleGroupsManagerFactory>;

  GoogleGroupsManagerFactory();
  ~GoogleGroupsManagerFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  bool ServiceIsCreatedWithBrowserState() const override;

  bool ServiceIsNULLWhileTesting() const override;

  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_
