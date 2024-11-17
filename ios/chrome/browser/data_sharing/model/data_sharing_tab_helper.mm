// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_tab_helper.h"

#import "base/check.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/data_sharing/model/ios_share_url_interception_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

DataSharingTabHelper::DataSharingTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

DataSharingTabHelper::~DataSharingTabHelper() = default;

void DataSharingTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  web::WebState* current_web_state = web_state();
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(current_web_state->GetBrowserState());
  data_sharing::DataSharingService* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);

  GURL url = net::GURLWithNSURL(request.URL);
  if (data_sharing_service &&
      data_sharing_service->ShouldInterceptNavigationForShareURL(url)) {
    BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);

    std::set<Browser*> regular_browsers =
        browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);
    Browser* current_browser = nullptr;
    if (regular_browsers.size() == 1) {
      current_browser = *regular_browsers.begin();
    } else {
      for (Browser* browser : regular_browsers) {
        if (browser->GetWebStateList()->GetIndexOfWebState(current_web_state) !=
            WebStateList::kInvalidIndex) {
          current_browser = browser;
          break;
        }
      }
    }

    CHECK(current_browser, base::NotFatalUntil::M138);

    auto context =
        std::make_unique<data_sharing::IOSShareURLInterceptionContext>(
            current_browser);
    data_sharing_service->HandleShareURLNavigationIntercepted(
        url, std::move(context));
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
