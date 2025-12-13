// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/data_sharing_browser_agent.h"

#import "base/check.h"
#import "components/collaboration/public/collaboration_service.h"
#import "ios/chrome/browser/collaboration/model/data_sharing_tab_helper.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/data_sharing/model/ios_share_url_interception_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

DataSharingBrowserAgent::DataSharingBrowserAgent(
    Browser* browser,
    collaboration::CollaborationService* service)
    : BrowserUserData(browser), service_(service) {
  CHECK(service_);
  StartObserving(browser, Policy::kAccordingToFeature);
}

DataSharingBrowserAgent::~DataSharingBrowserAgent() {
  StopObserving();
}

bool DataSharingBrowserAgent::IsAllowedToJoinSharedTabGroups() {
  return IsSharedTabGroupsJoinEnabled(service_);
}

void DataSharingBrowserAgent::HandleShareURLNavigationIntercepted(
    const GURL& url,
    collaboration::CollaborationServiceJoinEntryPoint entry) {
  service_->HandleShareURLNavigationIntercepted(
      url,
      std::make_unique<data_sharing::IOSShareURLInterceptionContext>(browser_),
      entry);
}

void DataSharingBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  DataSharingTabHelper::FromWebState(web_state)->SetDelegate(this);
}

void DataSharingBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  DataSharingTabHelper::FromWebState(web_state)->SetDelegate(nullptr);
}

void DataSharingBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  // Nothing to do.
}

void DataSharingBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  // Nothing to do.
}
