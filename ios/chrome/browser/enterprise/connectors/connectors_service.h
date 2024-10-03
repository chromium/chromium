// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_

#import "base/gtest_prod_util.h"
#import "components/enterprise/connectors/core/connectors_service_base.h"
#import "components/keyed_service/core/keyed_service.h"

namespace policy {
class UserCloudPolicyManager;
}  // namespace policy

namespace enterprise_connectors {

// iOS-specific implementation of `ConnectorsServiceBase`, to be used to access
// values for the following policies:
// - EnterpriseRealTimeUrlCheckMode
// - OnSecurityEventEnterpriseConnectors
class ConnectorsService : public ConnectorsServiceBase, public KeyedService {
 public:
  ConnectorsService(bool off_the_record,
                    PrefService* pref_service,
                    policy::UserCloudPolicyManager* user_cloud_policy_manager);

  // ConnectorsServiceBase:
  bool IsConnectorEnabled(AnalysisConnector connector) const override;

 protected:
  // ConnectorsServiceBase:
  std::optional<DmToken> GetDmToken(const char* scope_pref) const override;
  bool ConnectorsEnabled() const override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  ConnectorsManagerBase* GetConnectorsManagerBase() override;
  const ConnectorsManagerBase* GetConnectorsManagerBase() const override;
  policy::CloudPolicyManager* GetManagedUserCloudPolicyManager() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceTest, GetPrefs);
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceTest, GetProfileDmToken);
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceTest, GetBrowserDmToken);
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceTest, ConnectorsEnabled);

  bool off_the_record_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_
