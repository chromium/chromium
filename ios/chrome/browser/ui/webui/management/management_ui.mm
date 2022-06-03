// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/management/management_ui.h"

#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
web::WebUIIOSDataSource* CreateManagementUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIManagementHost);

  BrowserPolicyConnectorIOS* policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  bool is_managed =
      policy_connector && policy_connector->HasMachineLevelPolicies();
  source->AddString("isManaged", is_managed ? "true" : "false");
  source->AddString("learnMoreURL", kManagementLearnMoreURL);

  source->AddLocalizedString("managementMessage",
                             IDS_IOS_MANAGEMENT_UI_MESSAGE);
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
                               CreateManagementUIHTMLSource());
}

ManagementUI::~ManagementUI() {}
