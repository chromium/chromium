// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_UPGRADE_SERVICE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_UPGRADE_SERVICE_H_

#include <string>

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

// HttpsUpgradeService tracks the allowlist decisions for HTTPS-Only mode.
// Decisions are scoped to the host.
class HttpsUpgradeService : public KeyedService {
 public:
  // Returns whether |host| can be loaded over http://.
  virtual bool IsHttpAllowedForHost(const std::string& host) const = 0;

  // Allows future navigations to |host| over http://.
  virtual void AllowHttpForHost(const std::string& host) = 0;

  // Clears the persistent and in-memory allowlist entries. All of in-memory
  // entries are removed, but only persistent entries between delete_begin and
  // delete_end are removed.
  virtual void ClearAllowlist(base::Time delete_begin,
                              base::Time delete_end) = 0;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_UPGRADE_SERVICE_H_
