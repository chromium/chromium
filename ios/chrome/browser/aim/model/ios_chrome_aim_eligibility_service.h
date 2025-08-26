// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"

class PrefService;
class TemplateURLService;

namespace network {
class SharedURLLoaderFactory;
}

// Implements the AimEligibilityService on iOS, utilizing the platform's native
// country and locale detection to determine eligibility for AI mode features.
class IOSChromeAimEligibilityService : public AimEligibilityService {
 public:
  IOSChromeAimEligibilityService(
      PrefService* pref_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~IOSChromeAimEligibilityService() override;

  // AimEligibilityService:
  std::string GetCountryCode() const override;
  std::string GetLocale() const override;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_H_
