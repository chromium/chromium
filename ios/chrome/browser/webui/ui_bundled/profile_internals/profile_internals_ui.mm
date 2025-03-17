// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/profile_internals/profile_internals_ui.h"

#import <string>

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/profile_internals/profile_internals_handler.h"
#import "ios/chrome/grit/profile_internals_resources.h"
#import "ios/chrome/grit/profile_internals_resources_map.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

namespace {

// Creates a WebUI data source for the chrome://profile-internals page.
// Loosely equivalent to the non-iOS version at
// chrome/browser/ui/webui/profile_internals/profile_internals_ui.cc.
web::WebUIIOSDataSource* CreateProfileInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIProfileInternalsHost);

  source->AddResourcePath("", IDR_PROFILE_INTERNALS_PROFILE_INTERNALS_HTML);
  source->AddResourcePaths(kProfileInternalsResources);

  return source;
}

}  // namespace

ProfileInternalsUI::ProfileInternalsUI(web::WebUIIOS* web_ui,
                                       const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<ProfileInternalsHandler>());
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateProfileInternalsHTMLSource());
}

ProfileInternalsUI::~ProfileInternalsUI() {}
