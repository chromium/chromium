// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_FACTORY_H_
#define IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

// Singleton that owns all ConsentAuditors and associates them with
// ChromeBrowserState.
class ConsentAuditorFactory : public BrowserStateKeyedServiceFactory {
 public:
  static consent_auditor::ConsentAuditor* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static consent_auditor::ConsentAuditor* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);
  static ConsentAuditorFactory* GetInstance();

  ConsentAuditorFactory(const ConsentAuditorFactory&) = delete;
  ConsentAuditorFactory& operator=(const ConsentAuditorFactory&) = delete;

 private:
  friend class base::NoDestructor<ConsentAuditorFactory>;

  ConsentAuditorFactory();
  ~ConsentAuditorFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_FACTORY_H_
