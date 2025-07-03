// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_

#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"

namespace signin {
class IdentityManager;
}  // namespace signin

class PrefService;

// A browser-context keyed service for BWG.
class BwgService : public KeyedService {
 public:
  BwgService(signin::IdentityManager* identity_manager,
             PrefService* pref_service);
  ~BwgService() override;

  // Returns whether the current profile is eligible for BWG.
  bool IsEligibleForBwg();

 private:
  // Identity manager used to check account capabilities.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // The PrefService associated with the Profile.
  raw_ptr<PrefService> pref_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_
