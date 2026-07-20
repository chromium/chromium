// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_

#import "base/gtest_prod_util.h"
#import "components/enterprise/connectors/core/connectors_service_base.h"
#import "components/keyed_service/core/keyed_service.h"

namespace signin {
class IdentityManager;
}

namespace policy {
class UserCloudPolicyManager;
}

namespace enterprise_connectors {

// iOS-specific implementation of `ConnectorsServiceBase`, to be used to access
// values for the following policies:
// - EnterpriseRealTimeUrlCheckMode
// - OnSecurityEventEnterpriseConnectors
class ConnectorsService : public ConnectorsServiceBase, public KeyedService {
 public:
  ConnectorsService(PrefService* pref_service,
                    signin::IdentityManager* identity_manager,
                    policy::UserCloudPolicyManager* user_cloud_policy_manager,
                    const std::string& profile_name,
                    const base::FilePath& profile_path,
                    bool is_off_the_record);
  ~ConnectorsService() override;

  // Returns the CBCM domain or profile domain that enables connector policies.
  // If both set Connector policies, the CBCM domain is returned as it has
  // precedence.
  std::string GetManagementDomain();

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
  policy::CloudPolicyManager* GetManagedUserCloudPolicyManager() const override;

  bool IsProfileAffiliated() const override;
  std::string GetProfileEmail() const override;
  std::string GetDeviceClientId() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceTest, GetPrefs);
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceTest, GetProfileDmToken);
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceTest, GetBrowserDmToken);
  FRIEND_TEST_ALL_PREFIXES(ConnectorsServiceTest, ConnectorsEnabled);

  raw_ptr<PrefService> pref_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
  std::string profile_name_;
  base::FilePath profile_path_;
  bool is_off_the_record_ = false;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_SERVICE_H_
