// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_IOS_RULES_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_IOS_RULES_SERVICE_H_

#import "components/enterprise/data_controls/core/browser/action_context.h"
#import "components/enterprise/data_controls/core/browser/rules_service_base.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "url/gurl.h"

class ProfileIOS;

namespace data_controls {

// IOS-specific implementation of `data_controls::RulesServiceBase`.
class IOSRulesService : public RulesServiceBase {
 public:
  explicit IOSRulesService(ProfileIOS* profile);

  IOSRulesService(const IOSRulesService&) = delete;
  IOSRulesService& operator=(const IOSRulesService&) = delete;

  ~IOSRulesService() override;

  // Returns a clipboard verdict to be applied to a paste action. A nullptr
  // `source_profile` represents data coming from the OS clipboard.
  // `destionation_profile` is always expected to have a valid profile.
  virtual Verdict GetPasteVerdict(const GURL& source_url,
                                  const GURL& destionation_url,
                                  ProfileIOS* source_profile,
                                  ProfileIOS* destionation_profile);

 private:
  // RulesServiceBase override.
  bool incognito_profile() const override;

  // Helpers to help build ActionSource and ActionDestination.
  ActionSource GetAsActionSource(const GURL& source_url,
                                 ProfileIOS* source_profile) const;
  ActionDestination GetAsActionDestination(
      const GURL& destination_url,
      ProfileIOS* destination_profile) const;

  const raw_ptr<ProfileIOS> profile_ = nullptr;
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_IOS_RULES_SERVICE_H_
