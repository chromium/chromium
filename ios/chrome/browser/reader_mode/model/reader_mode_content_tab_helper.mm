// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_content_tab_helper.h"

#import "ios/chrome/browser/reader_mode/model/reader_mode_content_delegate.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/apple/url_conversions.h"

ReaderModeContentTabHelper::ReaderModeContentTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

ReaderModeContentTabHelper::~ReaderModeContentTabHelper() = default;

void ReaderModeContentTabHelper::SetDelegate(
    ReaderModeContentDelegate* delegate) {
  delegate_ = delegate;
}

void ReaderModeContentTabHelper::LoadContent(GURL content_url,
                                             NSData* content_data) {
  if (!web_state() || web_state()->IsBeingDestroyed()) {
    return;
  }
  content_url_ = content_url;
  content_url_request_allowed_ = false;
  web::NavigationManager* const navigation_manager =
      web_state()->GetNavigationManager();
  if (!navigation_manager->GetLastCommittedItem()) {
    // `LoadData` requires an already committed navigation item.
    std::vector<std::unique_ptr<web::NavigationItem>> navigation_items;
    navigation_items.push_back(web::NavigationItem::Create());
    navigation_manager->Restore(0, std::move(navigation_items));
  }
  web_state()->LoadData(content_data, @"text/html", std::move(content_url));
}

#pragma mark - WebStatePolicyDecider

void ReaderModeContentTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    RequestInfo request_info,
    PolicyDecisionCallback callback) {
  const GURL request_url = net::GURLWithNSURL(request.URL);
  if (request_url.EqualsIgnoringRef(content_url_) &&
      !content_url_request_allowed_) {
    // If the requested URL is the content URL and the request was not
    // previously allowed, allow the request.
    content_url_request_allowed_ = true;
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }
  // If the requested URL is NOT the content URL or it has been allowed already,
  // cancel the request
  std::move(callback).Run(PolicyDecision::Cancel());
  if (delegate_) {
    delegate_->ReaderModeContentDidCancelRequest(this, request, request_info);
  }
}
