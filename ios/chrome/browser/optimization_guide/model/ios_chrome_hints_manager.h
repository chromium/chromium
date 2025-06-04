// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_CHROME_HINTS_MANAGER_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_CHROME_HINTS_MANAGER_H_

#include "components/optimization_guide/core/hints_manager.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {

class IOSChromeHintsManager : public HintsManager {
 public:
  IOSChromeHintsManager(
      bool off_the_record,
      const std::string& application_locale,
      PrefService* pref_service,
      base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
      optimization_guide::TopHostProvider* top_host_provider,
      optimization_guide::TabUrlProvider* tab_url_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      OptimizationGuideLogger* optimization_guide_logger);

  ~IOSChromeHintsManager() override = default;

  IOSChromeHintsManager(const IOSChromeHintsManager&) = delete;
  IOSChromeHintsManager& operator=(const IOSChromeHintsManager&) = delete;
};

}  // namespace optimization_guide

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_CHROME_HINTS_MANAGER_H_
