// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/management/management_ui.h"

#import <optional>

#import "base/strings/utf_string_conversions.h"
#import "components/grit/components_resources.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/strings/grit/components_strings.h"
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

// Creates the HTML source for the chrome://management page.
web::WebUIIOSDataSource* CreateManagementUIHTMLSource(web::WebUIIOS* web_ui) {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIManagementHost);

  std::optional<std::u16string> management_message =
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
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateManagementUIHTMLSource(web_ui));
}

ManagementUI::~ManagementUI() {}
