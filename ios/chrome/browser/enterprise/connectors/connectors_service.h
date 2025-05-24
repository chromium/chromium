// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_

#import "base/gtest_prod_util.h"
#import "components/enterprise/connectors/core/connectors_service_base.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_manager.h"

class ProfileIOS;

namespace policy {
class UserCloudPolicyManager;
}  // namespace policy

namespace signin {
class IdentityManager;
}

namespace enterprise_connectors {

// iOS-specific implementation of `ConnectorsServiceBase`, to be used to access
// values for the following policies:
// - EnterpriseRealTimeUrlCheckMode
// - OnSecurityEventEnterpriseConnectors
class ConnectorsService : public ConnectorsServiceBase, public KeyedService {
 public:
  ConnectorsService(ProfileIOS* profile);
  ~ConnectorsService() override;

  // Returns the CBCM domain or profile domain that enables connector policies.
  // If both set Connector policies, the CBCM domain is returned as it has
  // precedence.
  std::string GetManagementDomain();

  // ConnectorsServiceBase:
  bool IsConnectorEnabled(AnalysisConnector connector) const override;
  // Returns the DM tokens corresponding to browser management, if one is
  // present.
  std::optional<std::string> GetBrowserDmToken() const override;
  std::unique_ptr<ClientMetadata> BuildClientMetadata(bool is_cloud) override;

  // Returns ClientMetadata populated with minimum required information
  std::unique_ptr<ClientMetadata> GetBasicClientMetadata();

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

  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<ConnectorsManager> connectors_manager_;
  // Unowned pointer used for retrieving the management domain for connectors
  // policies. Can be null for incognito profiles.
  raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_
