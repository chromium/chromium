// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/management/management_ui.h"

#import "base/strings/string_split.h"
#import "base/strings/utf_string_conversions.h"
#import "components/account_id/account_id.h"
#import "components/grit/components_resources.h"
#import "components/policy/core/browser/webui/policy_data_utils.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/account_managed_status_finder.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/policy/user_policy_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/l10n/l10n_util.h"

using signin::AccountManagedStatusFinder;

namespace {

// Returns the domain of the machine level cloud policy. Returns absl::nullopt
// if the domain cannot be retrieved (eg. because there are no machine level
// policies).
absl::optional<std::string> GetMachineLevelPolicyDomain() {
  policy::MachineLevelUserCloudPolicyManager* manager =
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->machine_level_user_cloud_policy_manager();
  return policy::GetManagedBy(manager);
}

// Gets the AccountId from the provided `account_info`.
AccountId AccountIdFromAccountInfo(const CoreAccountInfo& account_info) {
  if (account_info.email.empty() || account_info.gaia.empty()) {
    return EmptyAccountId();
  }

  return AccountId::FromUserEmailGaiaId(
      gaia::CanonicalizeEmail(account_info.email), account_info.gaia);
}

// Extracts the domain from the email. Returns absl::nullopt if there is no
// domain.
absl::optional<std::string> ExtractDomainFromEmail(const std::string& email) {
  std::vector<std::string> components = base::SplitString(
      email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (components.size() != 2) {
    return absl::nullopt;
  }
  const std::string domain = components[1];
  if (domain.empty()) {
    return absl::nullopt;
  }

  return domain;
}

// Returns the domain of the user cloud policy. Returns absl::nullopt if the
// domain cannot be retrieved (eg. because there is no user policy).
absl::optional<std::string> GetUserPolicyDomain(web::WebUIIOS* web_ui) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromWebUIIOS(web_ui)->GetOriginalChromeBrowserState();

  if (!CanFetchUserPolicy(
          AuthenticationServiceFactory::GetForBrowserState(browser_state),
          browser_state->GetPrefs())) {
    return absl::nullopt;
  }

  AccountId account_id = AccountIdFromAccountInfo(
      IdentityManagerFactory::GetForBrowserState(browser_state)
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  const std::string user_email = account_id.GetUserEmail();

  if (user_email.empty()) {
    return absl::nullopt;
  }

  if (AccountManagedStatusFinder::IsEnterpriseUserBasedOnEmail(user_email) ==
      AccountManagedStatusFinder::EmailEnterpriseStatus::kKnownNonEnterprise) {
    return absl::nullopt;
  }

  return ExtractDomainFromEmail(user_email);
}

// Returns the management message depending on the levels of the policies that
// are applied. Returns absl::nullopt if there are no policies.
absl::optional<std::u16string> GetManagementMessage(web::WebUIIOS* web_ui) {
  absl::optional<std::string> machine_level_policy_domain =
      GetMachineLevelPolicyDomain();
  absl::optional<std::string> user_policy_domain = GetUserPolicyDomain(web_ui);

  if (machine_level_policy_domain && user_policy_domain) {
    if (machine_level_policy_domain == user_policy_domain) {
      // Return a message with a single domain for both the profile and the
      // browser when the domains are the same. Any domain can be used in that
      // case.
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_SAME_MANAGED_BY,
          base::UTF8ToUTF16(*machine_level_policy_domain));
    } else {
      // Return a message with both domains when they are different.
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY,
          base::UTF8ToUTF16(*machine_level_policy_domain),
          base::UTF8ToUTF16(*user_policy_domain));
    }
  }

  if (machine_level_policy_domain) {
    return l10n_util::GetStringFUTF16(
        IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
        base::UTF8ToUTF16(*machine_level_policy_domain));
  }

  if (user_policy_domain) {
    return l10n_util::GetStringFUTF16(
        IDS_MANAGEMENT_SUBTITLE_PROFILE_MANAGED_BY,
        base::UTF8ToUTF16(*user_policy_domain));
  }

  BrowserPolicyConnectorIOS* policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  bool has_machine_level_policy =
      policy_connector && policy_connector->HasMachineLevelPolicies();
  if (has_machine_level_policy) {
    // Return a message without the domain if there are policies on the machine
    // but couldn't obtain the domain. This can happen when using MDM.
    return l10n_util::GetStringUTF16(IDS_IOS_MANAGEMENT_UI_MESSAGE);
  }

  return absl::nullopt;
}

// Creates the HTML source for the chrome://management page.
web::WebUIIOSDataSource* CreateManagementUIHTMLSource(web::WebUIIOS* web_ui) {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIManagementHost);

  absl::optional<std::u16string> management_message =
      GetManagementMessage(web_ui);

  source->AddString("isManaged", management_message ? "true" : "false");
  source->AddString("learnMoreURL", kManagementLearnMoreURL);

  source->AddString("managementMessage",
                    management_message.value_or(std::u16string()));
  source->AddLocalizedString("managedInfo", IDS_IOS_MANAGEMENT_UI_DESC);
  source->AddLocalizedString("unmanagedInfo",
                             IDS_IOS_MANAGEMENT_UI_UNMANAGED_DESC);
  source->AddLocalizedString("learnMore",
                             IDS_IOS_MANAGEMENT_UI_LEARN_MORE_LINK);

  source->UseStringsJs();
  source->AddResourcePath("management.css", IDR_MOBILE_MANAGEMENT_CSS);
  source->AddResourcePath("management.js", IDR_MOBILE_MANAGEMENT_JS);
  source->SetDefaultResource(IDR_MOBILE_MANAGEMENT_HTML);
  return source;
}

}  // namespace

ManagementUI::ManagementUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web::WebUIIOSDataSource::Add(ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreateManagementUIHTMLSource(web_ui));
}

ManagementUI::~ManagementUI() {}
