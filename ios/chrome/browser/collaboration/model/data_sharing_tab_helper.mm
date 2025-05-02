// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/data_sharing_tab_helper.h"

#import "base/check.h"
#import "components/collaboration/public/collaboration_flow_entry_point.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/data_sharing/public/data_sharing_utils.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/data_sharing/model/ios_share_url_interception_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Return whether the navigation should be handled if it is a share URL.
bool ShouldHandleShareURLNavigation(
    web::WebStatePolicyDecider::RequestInfo request_info) {
  // Make sure to keep it in sync between platforms.
  // LINT.IfChange(ShouldHandleShareURLNavigation)
  if (!request_info.target_frame_is_main) {
    return false;
  }

  if (request_info.is_user_initiated && !request_info.user_tapped_recently) {
    return false;
  }

  return true;
  // LINT.ThenChange(/chrome/browser/data_sharing/data_sharing_navigation_throttle.cc:ShouldHandleShareURLNavigation)
}

}  // namespace

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
  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);

  GURL url = net::GURLWithNSURL(request.URL);
  if (collaboration_service &&
      data_sharing::DataSharingUtils::ShouldInterceptNavigationForShareURL(
          url)) {
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

    if (ShouldHandleShareURLNavigation(request_info)) {
      auto context =
          std::make_unique<data_sharing::IOSShareURLInterceptionContext>(
              current_browser);
      collaboration_service->HandleShareURLNavigationIntercepted(
          url, std::move(context),
          collaboration::GetEntryPointFromPageTransition(
              request_info.transition_type));
    }
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
