// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/status_provider/user_cloud_policy_status_provider.h"

#import <string>
#import <vector>

#import "base/containers/flat_set.h"
#import "base/strings/string_split.h"
#import "base/values.h"
#import "components/policy/core/common/cloud/affiliation.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"

namespace {

// Extracts the domain name from the username. Only works with the
// username@domain format, returns std::nullopt otherwise.
std::optional<std::string> ExtractDomainName(std::string_view username) {
  std::vector<std::string> pieces = base::SplitString(
      username, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (pieces.size() != 2) {
    return std::nullopt;
  }
  return pieces.at(1);
}

// Sets the domain based on the username if there is a username set and the
// username is in the correct format.
void SetDomainExtractedFromUsername(base::Value::Dict* status_dict) {
  const std::string* username = status_dict->FindString(policy::kUsernameKey);
  if (!username) {
    return;
  }
  if (const auto domain = ExtractDomainName(*username)) {
    status_dict->Set(policy::kDomainKey, *domain);
  }
}

}  // namespace

UserCloudPolicyStatusProvider::UserCloudPolicyStatusProvider(
    UserCloudPolicyStatusProvider::Delegate* delegate,
    policy::CloudPolicyCore* user_level_policy_core,
    signin::IdentityManager* identity_manager)
    : delegate_(delegate),
      user_level_policy_core_(user_level_policy_core),
      identity_manager_(identity_manager) {
  CHECK(user_level_policy_core_);

  core_observation_.Observe(user_level_policy_core_);
  store_observation_.Observe(user_level_policy_core_->store());
  if (user_level_policy_core_->client()) {
    client_observation_.Observe(user_level_policy_core_->client());
  }
}

base::Value::Dict UserCloudPolicyStatusProvider::GetStatus() {
  // Determine if need to show flex org warning.
  AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  const bool show_flex_org_warning = account_info.IsMemberOfFlexOrg();

  if (!user_level_policy_core_->store()->is_managed() &&
      !show_flex_org_warning) {
    // Do not provide any user cloud policy status if the domain isn't managed
    // AND the account isn't a member of a flex org. If the latter is true,
    // it is okay to show some status information even if there is no active
    // policy data (e.g. showing the flex org warning).
    return {};
  }

  // Get status information at this point even if there is no policy data in the
  // case of flex orgs (for which we still want to shown a minimal amount of
  // information).

  // Set the status payload.
  // TODO(b/310636701): Set the Profile ID once it is used on iOS.
  base::Value::Dict dict =
      policy::PolicyStatusProvider::GetStatusFromCore(user_level_policy_core_);
  SetDomainExtractedFromUsername(&dict);
  dict.Set("isAffiliated", IsAffiliated());
  dict.Set(policy::kFlexOrgWarningKey, show_flex_org_warning);
  dict.Set(policy::kPolicyDescriptionKey, "statusUser");
  return dict;
}

UserCloudPolicyStatusProvider::~UserCloudPolicyStatusProvider() {}

void UserCloudPolicyStatusProvider::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void UserCloudPolicyStatusProvider::OnStoreError(
    policy::CloudPolicyStore* store) {
  NotifyStatusChange();
}

void UserCloudPolicyStatusProvider::OnCoreConnected(
    policy::CloudPolicyCore* core) {
  client_observation_.Reset();
  client_observation_.Observe(core->client());
}

void UserCloudPolicyStatusProvider::OnRefreshSchedulerStarted(
    policy::CloudPolicyCore* core) {}

void UserCloudPolicyStatusProvider::OnCoreDisconnecting(
    policy::CloudPolicyCore* core) {
  client_observation_.Reset();
}

void UserCloudPolicyStatusProvider::OnPolicyFetched(
    policy::CloudPolicyClient* client) {
  NotifyStatusChange();
}

void UserCloudPolicyStatusProvider::OnRegistrationStateChanged(
    policy::CloudPolicyClient* client) {
  NotifyStatusChange();
}

void UserCloudPolicyStatusProvider::OnClientError(
    policy::CloudPolicyClient* client) {
  NotifyStatusChange();
}

bool UserCloudPolicyStatusProvider::IsAffiliated() {
  return policy::IsAffiliated(
      /*user_ids=*/policy::GetAffiliationIdsFromCore(*user_level_policy_core_,
                                                     /*for_device=*/false),
      delegate_->GetDeviceAffiliationIds());
}
