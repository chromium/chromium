// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_IOS_RULES_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_IOS_RULES_SERVICE_H_

#import "components/enterprise/data_controls/core/browser/rules_service_base.h"

class ProfileIOS;

namespace data_controls {

// IOS-specific implementation of `data_controls::RulesServiceBase`.
class IOSRulesService : public RulesServiceBase {
 public:
  explicit IOSRulesService(ProfileIOS* profile);

  IOSRulesService(const IOSRulesService&) = delete;
  IOSRulesService& operator=(const IOSRulesService&) = delete;

  ~IOSRulesService() override;

 private:
  // RulesServiceBase override.
  bool incognito_profile() const override;

  const raw_ptr<ProfileIOS> profile_ = nullptr;
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_IOS_RULES_SERVICE_H_
