// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_tab_helper.h"

#import "components/data_sharing/public/data_sharing_service.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

DataSharingTabHelper::DataSharingTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

DataSharingTabHelper::~DataSharingTabHelper() = default;

void DataSharingTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state()->GetBrowserState());
  data_sharing::DataSharingService* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);

  GURL url = net::GURLWithNSURL(request.URL);
  if (data_sharing_service &&
      data_sharing_service->ShouldInterceptNavigationForShareURL(url)) {
    data_sharing_service->HandleShareURLNavigationIntercepted(url);
    std::move(callback).Run(PolicyDecision::Cancel());

    // Close the tab if the url interception ends with an empty page.
    const GURL& last_committed_url = web_state()->GetLastCommittedURL();
    if (!last_committed_url.is_valid() || last_committed_url.IsAboutBlank() ||
        last_committed_url.is_empty()) {
      web_state()->CloseWebState();
    }
    return;
  }

  std::move(callback).Run(PolicyDecision::Allow());
}

WEB_STATE_USER_DATA_KEY_IMPL(DataSharingTabHelper)
