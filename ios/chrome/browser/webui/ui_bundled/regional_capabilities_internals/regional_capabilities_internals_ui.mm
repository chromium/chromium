// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/regional_capabilities_internals/regional_capabilities_internals_ui.h"

#import "components/grit/regional_capabilities_internals_resources.h"
#import "components/grit/regional_capabilities_internals_resources_map.h"
#import "components/regional_capabilities/access/country_access_reason.h"
#import "components/regional_capabilities/regional_capabilities_internals_data_holder.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/webui/regional_capabilities_internals/constants.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

using regional_capabilities::CountryAccessKey;
using regional_capabilities::CountryAccessReason;

RegionalCapabilitiesInternalsUI::RegionalCapabilitiesInternalsUI(
    web::WebUIIOS* web_ui,
    const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web::WebUIIOSDataSource* source = web::WebUIIOSDataSource::Create(
      regional_capabilities::kChromeUIRegionalCapabilitiesInternalsHost);
  source->UseStringsJs();
  source->AddResourcePath("", IDR_REGIONAL_CAPABILITIES_INTERNALS_INDEX_HTML);
  source->AddResourcePaths(kRegionalCapabilitiesInternalsResources);

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  regional_capabilities::InternalsDataHolder internals_data_holder =
      ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile)
          ->GetInternalsData();
  for (const auto& [key, value] :
       internals_data_holder.GetRestricted(CountryAccessKey(
           CountryAccessReason::
               kRegionalCapabilitiesInternalsDisplayInDebugUi))) {
    source->AddString(key, value);
  }

  web::WebUIIOSDataSource::Add(profile, source);
}

RegionalCapabilitiesInternalsUI::~RegionalCapabilitiesInternalsUI() = default;
