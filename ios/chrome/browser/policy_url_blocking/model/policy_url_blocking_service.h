// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_SERVICE_H_
#define IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_SERVICE_H_

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// Associates a policy::URLBlocklistManager instance with a BrowserState.
class PolicyBlocklistService : public KeyedService {
 public:
  explicit PolicyBlocklistService(
      web::BrowserState* browser_state,
      std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager);
  ~PolicyBlocklistService() override;

  // Returns the blocking state for `url`.
  policy::URLBlocklist::URLBlocklistState GetURLBlocklistState(
      const GURL& url) const;

 private:
  // The URLBlocklistManager associated with `browser_state`.
  std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager_;

  PolicyBlocklistService(const PolicyBlocklistService&) = delete;
  PolicyBlocklistService& operator=(const PolicyBlocklistService&) = delete;
};

class PolicyBlocklistServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static PolicyBlocklistServiceFactory* GetInstance();
  static PolicyBlocklistService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<PolicyBlocklistServiceFactory>;

  PolicyBlocklistServiceFactory();
  ~PolicyBlocklistServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* browser_state) const override;

  PolicyBlocklistServiceFactory(const PolicyBlocklistServiceFactory&) = delete;
  PolicyBlocklistServiceFactory& operator=(
      const PolicyBlocklistServiceFactory&) = delete;
};

#endif  // IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_SERVICE_H_
