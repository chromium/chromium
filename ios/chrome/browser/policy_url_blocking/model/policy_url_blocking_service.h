// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_SERVICE_H_
#define IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"

// Owns a policy::URLBlocklistManager.
class PolicyBlocklistService : public KeyedService {
 public:
  explicit PolicyBlocklistService(
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

#endif  // IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_SERVICE_H_
