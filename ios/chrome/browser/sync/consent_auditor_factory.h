// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_CONSENT_AUDITOR_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_CONSENT_AUDITOR_FACTORY_H_

// TODO(crbug.com/850428): Move this and .cc back to
// ios/chrome/browser/consent_auditor, when it does not depend on
// UserEventService anymore. Currently this is not possible due to a BUILD.gn
// dependency.

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// Singleton that owns all ConsentAuditors and associates them with
// ios::ChromeBrowserState.
class ConsentAuditorFactory : public BrowserStateKeyedServiceFactory {
 public:
  static consent_auditor::ConsentAuditor* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static consent_auditor::ConsentAuditor* GetForBrowserStateIfExists(
      ios::ChromeBrowserState* browser_state);
  static ConsentAuditorFactory* GetInstance();

 private:
  friend class base::NoDestructor<ConsentAuditorFactory>;

  ConsentAuditorFactory();
  ~ConsentAuditorFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(ConsentAuditorFactory);
};

#endif  // IOS_CHROME_BROWSER_SYNC_CONSENT_AUDITOR_FACTORY_H_
