// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_MODEL_DOMAIN_DIVERSITY_REPORTER_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_MODEL_DOMAIN_DIVERSITY_REPORTER_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class DomainDiversityReporter;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Singleton that creates all DomainDiversityReporter instances and associates
// them with BrowserState.
class DomainDiversityReporterFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static DomainDiversityReporter* GetForProfile(ProfileIOS* profile);

  static DomainDiversityReporterFactory* GetInstance();

 private:
  friend class base::NoDestructor<DomainDiversityReporterFactory>;

  DomainDiversityReporterFactory();
  ~DomainDiversityReporterFactory() override;

  // BrowserStateKeyedServiceFactory implementation
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_HISTORY_MODEL_DOMAIN_DIVERSITY_REPORTER_FACTORY_H_
