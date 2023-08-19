// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/webui/url_data_source_ios.h"

#import "ios/web/public/web_client.h"
#import "ios/web/webui/url_data_manager_ios.h"
#import "net/url_request/url_request.h"

namespace web {

void URLDataSourceIOS::Add(BrowserState* browser_state,
                           URLDataSourceIOS* source) {
  URLDataManagerIOS::AddDataSource(browser_state, source);
}

bool URLDataSourceIOS::ShouldReplaceExistingSource() const {
  return true;
}

bool URLDataSourceIOS::AllowCaching() const {
  return true;
}

std::string URLDataSourceIOS::GetContentSecurityPolicyObjectSrc() const {
  return "object-src 'none';";
}

bool URLDataSourceIOS::ShouldDenyXFrameOptions() const {
  return true;
}

bool URLDataSourceIOS::ShouldServiceRequest(const GURL& url) const {
  return GetWebClient()->IsAppSpecificURL(url);
}

bool URLDataSourceIOS::ShouldReplaceI18nInJS() const {
  return false;
}

}  // namespace web
