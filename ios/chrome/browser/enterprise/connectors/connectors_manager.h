// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#import "components/enterprise/connectors/core/connectors_manager_base.h"
#import "components/enterprise/connectors/core/service_provider_config.h"

namespace enterprise_connectors {

// iOS-specific implementation of `ConnectorsManagerBase`. This should only be
// used by the iOS implementation of `ConnectorsService`.
class ConnectorsManager : public ConnectorsManagerBase {
 public:
  ConnectorsManager(PrefService* pref_service,
                    const ServiceProviderConfig* config);
  ~ConnectorsManager() override;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
