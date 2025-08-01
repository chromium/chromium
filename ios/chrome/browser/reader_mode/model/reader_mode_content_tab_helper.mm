// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_content_tab_helper.h"

#import "ios/chrome/browser/browser_container/model/edit_menu_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_tab_helper.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_content_delegate.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "net/base/apple/url_conversions.h"

ReaderModeContentTabHelper::ReaderModeContentTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {
  web_state_observation_.Observe(web_state);
}

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

void ReaderModeContentTabHelper::AttachSupportedTabHelpers(
    web::WebState* original_web_state) {
  // The Reader mode content WebState should not attach tab helpers for
  // features which are not supported by the original WebState.
  EditMenuTabHelper* main_tab_edit_menu_builder_tab_helper =
      EditMenuTabHelper::FromWebState(original_web_state);
  if (main_tab_edit_menu_builder_tab_helper) {
    EditMenuTabHelper::CreateForWebState(web_state());
    EditMenuTabHelper::FromWebState(web_state())
        ->SetEditMenuBuilder(
            main_tab_edit_menu_builder_tab_helper->GetEditMenuBuilder());
    WebSelectionTabHelper::CreateForWebState(web_state());
    LinkToTextTabHelper::CreateForWebState(web_state());
  }

  FindTabHelper::CreateForWebState(web_state());
  ImageFetchTabHelper::CreateForWebState(web_state());
  OverlayRequestQueue::CreateForWebState(web_state());
  web_state_delegate_ = std::make_unique<ReaderModeWebStateDelegate>(
      original_web_state->GetDelegate());
  web_state()->SetDelegate(web_state_delegate_.get());
}

void ReaderModeContentTabHelper::SetFullscreenController(
    FullscreenController* fullscreen_controller) {
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebState(web_state());
  if (!find_tab_helper) {
    return;
  }
  find_tab_helper->SetFullscreenController(fullscreen_controller);
}

#pragma mark - WebStatePolicyDecider

void ReaderModeContentTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    RequestInfo request_info,
    PolicyDecisionCallback callback) {
  const GURL request_url = net::GURLWithNSURL(request.URL);
  if ((request_url.EqualsIgnoringRef(content_url_) &&
       !content_url_request_allowed_) ||
      !request_info.target_frame_is_main) {
    // If the requested URL is the content and was not already allowed,
    // or if the request does not target the main frame, then allow it.
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

void ReaderModeContentTabHelper::WebStateDestroyed() {
  web_state_observation_.Reset();
}

#pragma mark - WebStateObserver

void ReaderModeContentTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (delegate_) {
    delegate_->ReaderModeContentDidLoadData(this);
  }
}

void ReaderModeContentTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}
