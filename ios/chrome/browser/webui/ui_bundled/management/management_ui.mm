// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/management/management_ui.h"

#import <optional>

#import "base/strings/strcat.h"
#import "base/strings/utf_string_conversions.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/grit/management_resources.h"
#import "components/grit/management_resources_map.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using enterprise_connectors::ConnectorsService;

// Returns the management message depending on the levels of the policies that
// are applied. Returns std::nullopt if there are no policies.
std::optional<std::u16string> GetManagementMessage(web::WebUIIOS* web_ui) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui)->GetOriginalProfile();
  ManagementState state =
      GetManagementState(IdentityManagerFactory::GetForProfile(profile),
                         AuthenticationServiceFactory::GetForProfile(profile),
                         profile->GetPrefs());

  if (state.machine_level_domain && state.user_level_domain) {
    if (state.machine_level_domain == state.user_level_domain) {
      // Return a message with a single domain for both the profile and the
      // browser when the domains are the same. Any domain can be used in that
      // case.
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_SAME_MANAGED_BY,
          base::UTF8ToUTF16(*state.machine_level_domain));
    } else {
      // Return a message with both domains when they are different.
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY,
          base::UTF8ToUTF16(*state.machine_level_domain),
          base::UTF8ToUTF16(*state.user_level_domain));
    }
  }

  if (state.machine_level_domain) {
    return l10n_util::GetStringFUTF16(
        IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
        base::UTF8ToUTF16(*state.machine_level_domain));
  }

  if (state.user_level_domain) {
    return l10n_util::GetStringFUTF16(
        IDS_MANAGEMENT_SUBTITLE_PROFILE_MANAGED_BY,
        base::UTF8ToUTF16(*state.user_level_domain));
  }

  if (state.is_managed()) {
    // Return a message without the domain if there are policies on the machine
    // but couldn't obtain the domain. This can happen when using MDM.
    return l10n_util::GetStringUTF16(IDS_IOS_MANAGEMENT_UI_MESSAGE);
  }

  return std::nullopt;
}

// Whether the Browser Reporting section should be displayed. This section is
// visible if reporting is enabled at the browser level.
bool IsBrowserReportingEnabled(PrefService* local_state) {
  return local_state->GetBoolean(enterprise_reporting::kCloudReportingEnabled);
}

// Whether the Profile Reporting section should be displayed. This section is
// visible if reporting is enabled at the profile level.
bool IsProfileReportingEnabled(PrefService* profile_prefs) {
  return profile_prefs->GetBoolean(
      enterprise_reporting::kCloudProfileReportingEnabled);
}

// Whether the "Page is visited" event subsection under Chrome Enteprise
// Connectors should be displayed. This subsection is visible if Enterprise Url
// filtering is enabled.
bool IsPageVisitEventEnabled(ConnectorsService* connectors_service) {
  return connectors_service &&
         enterprise_connectors::IsEnterpriseUrlFilteringEnabled(
             connectors_service->GetAppliedRealTimeUrlCheck());
}

// Whether the "Security event occurs" event subsection under Chrome Enteprise
// Connectors should be displayed. This subsection is visible if Enterprise
// Event Reporting is enabled.
bool IsSecurityEventEnabled(ConnectorsService* connectors_service) {
  return !connectors_service->GetReportingServiceProviderNames().empty();
}

// Returns the message explaining that Chrome Enterprise Connectors are turned
// on.
std::u16string GetConnectorsSectionDescription(
    ConnectorsService* connectors_service) {
  const std::string enterprise_manager =
      connectors_service->GetManagementDomain();

  return enterprise_manager.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION)
             : l10n_util::GetStringFUTF16(
                   IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION_BY,
                   base::UTF8ToUTF16(enterprise_manager));
}

// Helper for building the title of a Chrome Enterprise Connectors event
// subsection.
std::u16string GetEventTitle(int event_title_id) {
  return base::StrCat(
      {l10n_util::GetStringUTF16(IDS_MANAGEMENT_CONNECTORS_EVENT), u": ",
       l10n_util::GetStringUTF16(event_title_id)});
}

// Helper for building the description of a Chrome Enterprise Connectors event
// subsection.
std::u16string GetEventDescription(int event_description_id) {
  return base::StrCat(
      {l10n_util::GetStringUTF16(IDS_MANAGEMENT_CONNECTORS_VISIBLE_DATA), u": ",
       l10n_util::GetStringUTF16(event_description_id)});
}

// Title for the Chrome Enterprise Connectors Page Visit event subsection.
std::u16string GetPageVisitEventTitle() {
  return GetEventTitle(IDS_MANAGEMENT_PAGE_VISITED_EVENT);
}

// Description for the Chrome Enterprise Connectors Page Visit event subsection.
std::u16string GetPageVisitEventDescription() {
  return GetEventDescription(IDS_MANAGEMENT_PAGE_VISITED_VISIBLE_DATA);
}

// Title for the Chrome Enterprise Connectors Security event subsection.
std::u16string GetSecurityEventTitle() {
  return GetEventTitle(IDS_MANAGEMENT_ENTERPRISE_REPORTING_EVENT);
}

// Description for the Chrome Enterprise Connectors Security event subsection.
std::u16string GetSecurityEventDescription() {
  return GetEventDescription(IDS_MANAGEMENT_ENTERPRISE_REPORTING_VISIBLE_DATA);
}

// Creates the HTML source for the chrome://management page.
web::WebUIIOSDataSource* CreateManagementUIHTMLSource(web::WebUIIOS* web_ui) {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIManagementHost);

  std::optional<std::u16string> management_message =
      GetManagementMessage(web_ui);

  source->AddBoolean("isManaged", management_message.has_value());
  source->AddString("learnMoreURL", kManagementLearnMoreURL);

  source->AddString("managementMessage",
                    management_message.value_or(std::u16string()));
  source->AddLocalizedString("managedInfo", IDS_IOS_MANAGEMENT_UI_DESC);
  source->AddLocalizedString("unmanagedInfo",
                             IDS_IOS_MANAGEMENT_UI_UNMANAGED_DESC);
  source->AddLocalizedString("learnMore",
                             IDS_IOS_MANAGEMENT_UI_LEARN_MORE_LINK);

  // Reporting Section
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui)->GetOriginalProfile();
  source->AddLocalizedString("browserReporting",
                             IDS_MANAGEMENT_BROWSER_REPORTING);

  // Browser reporting
  source->AddBoolean(
      "browserReportingEnabled",
      IsBrowserReportingEnabled(GetApplicationContext()->GetLocalState()));
  source->AddLocalizedString("browserReportingExplanation",
                             IDS_MANAGEMENT_BROWSER_REPORTING_EXPLANATION);
  source->AddLocalizedString("browserReportingOverview",
                             IDS_MANAGEMENT_BROWSER_REPORTING_OVERVIEW);
  source->AddLocalizedString(
      "browserReportingDeviceInformation",
      IDS_MANAGEMENT_BROWSER_REPORTING_DEVICE_INFORMATION);
  source->AddLocalizedString(
      "browserReportingDeviceInformationContinued",
      IDS_MANAGEMENT_BROWSER_REPORTING_DEVICE_INFORMATION_CONTINUED);
  source->AddLocalizedString(
      "browserReportingBrowserAndProfiles",
      IDS_MANAGEMENT_BROWSER_REPORTING_BROWSER_AND_PROFILES);
  source->AddLocalizedString(
      "browserReportingBrowserAndProfilesContinued",
      IDS_MANAGEMENT_BROWSER_REPORTING_BROWSER_AND_PROFILES_CONTINUED);
  source->AddLocalizedString("browserReportingPolicy",
                             IDS_MANAGEMENT_BROWSER_REPORTING_POLICY);
  source->AddLocalizedString("browserReportingLearnMore",
                             IDS_MANAGEMENT_BROWSER_REPORTING_LEARN_MORE);

  // Profile reporting
  source->AddBoolean("profileReportingEnabled",
                     IsProfileReportingEnabled(profile->GetPrefs()));
  source->AddLocalizedString("profileReportingExplanation",
                             IDS_MANAGEMENT_PROFILE_REPORTING_EXPLANATION);
  source->AddLocalizedString("profileReportingOverview",
                             IDS_MANAGEMENT_PROFILE_REPORTING_OVERVIEW);
  source->AddLocalizedString("profileReportingUsername",
                             IDS_MANAGEMENT_PROFILE_REPORTING_USERNAME);
  source->AddLocalizedString("profileReportingBrowser",
                             IDS_MANAGEMENT_PROFILE_REPORTING_BROWSER);
  source->AddLocalizedString("profileReportingPolicy",
                             IDS_MANAGEMENT_PROFILE_REPORTING_POLICY);
  source->AddLocalizedString("profileReportingLearnMore",
                             IDS_MANAGEMENT_PROFILE_REPORTING_LEARN_MORE);

  // Connectors Section
  auto* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForProfile(profile);
  CHECK(connectors_service);

  source->AddLocalizedString("connectorsSectionTitle",
                             IDS_MANAGEMENT_THREAT_PROTECTION);
  source->AddString("connectorsDescription",
                    GetConnectorsSectionDescription(connectors_service));

  source->AddBoolean("pageVisitEventEnabled",
                     IsPageVisitEventEnabled(connectors_service));
  source->AddString("pageVisitEventTitle", GetPageVisitEventTitle());
  source->AddString("pageVisitEventData", GetPageVisitEventDescription());

  source->AddBoolean("securityEventEnabled",
                     IsSecurityEventEnabled(connectors_service));
  source->AddString("securityEventTitle", GetSecurityEventTitle());
  source->AddString("securityEventData", GetSecurityEventDescription());

  source->UseStringsJs();
  source->AddResourcePaths(kManagementResources);
  source->AddResourcePath("", IDR_MANAGEMENT_MANAGEMENT_HTML);
  return source;
}

}  // namespace

ManagementUI::ManagementUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateManagementUIHTMLSource(web_ui));
}

ManagementUI::~ManagementUI() {}
