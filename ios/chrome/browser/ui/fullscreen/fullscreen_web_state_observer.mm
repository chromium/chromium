// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_observer.h"

#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_proxy_observer.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/public/provider/chrome/browser/fullscreen/fullscreen_api.h"
#import "ios/web/common/url_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

FullscreenWebStateObserver::FullscreenWebStateObserver(
    FullscreenController* controller,
    FullscreenModel* model,
    FullscreenMediator* mediator)
    : controller_(controller),
      model_(model),
      mediator_(mediator),
      web_view_proxy_observer_([[FullscreenWebViewProxyObserver alloc]
          initWithModel:model_
               mediator:mediator]) {
  DCHECK(controller_);
  DCHECK(model_);
}

FullscreenWebStateObserver::~FullscreenWebStateObserver() = default;

void FullscreenWebStateObserver::SetWebState(web::WebState* web_state) {
  if (web_state_ == web_state)
    return;
  if (web_state_)
    web_state_->RemoveObserver(this);
  web_state_ = web_state;
  if (web_state_) {
    web_state_->AddObserver(this);
    // The toolbar should be visible whenever the current tab changes.
    model_->ResetForNavigation();
  }
  mediator_->SetWebState(web_state);
  // Update the scroll view replacement handler's proxy.
  web_view_proxy_observer_.proxy =
      web_state_ ? web_state_->GetWebViewProxy() : nil;
}

void FullscreenWebStateObserver::WasShown(web::WebState* web_state) {
  // Show the toolbars when a WebState is shown.
  model_->ResetForNavigation();
}

void FullscreenWebStateObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  const GURL& navigation_url = navigation_context->GetUrl();
  bool url_changed = web::GURLByRemovingRefFromGURL(navigation_url) !=
                     web::GURLByRemovingRefFromGURL(last_navigation_url_);
  last_navigation_url_ = navigation_url;

  // Due to limitations in WKWebView's rendering, different MIME types must be
  // inset using different techniques:
  // - PDFs need to be inset using the scroll view's `contentInset` property or
  //   the floating page indicator is laid out incorrectly.
  // - For normal pages, using `contentInset` breaks the layout of fixed-
  //   position DOM elements, so top padding must be accomplished by updating
  //   the WKWebView's frame.
  bool is_pdf = web_state->GetContentsMimeType() == "application/pdf";
  id<CRWWebViewProxy> web_view_proxy = web_state->GetWebViewProxy();
  web_view_proxy.shouldUseViewContentInset = is_pdf;

  model_->SetResizesScrollView(
      !is_pdf && !ios::provider::IsFullscreenSmoothScrollingSupported());

  model_->SetScrollViewHeight(web_state->GetView().bounds.size.height);

  // Only reset the model for document-changing navigations or same-document
  // navigations that update the visible URL.
  if (!navigation_context->IsSameDocument() || url_changed) {
    model_->ResetForNavigation();
  }
}

void FullscreenWebStateObserver::DidStartLoading(web::WebState* web_state) {
  // This is done to show the toolbar when navigating to a page that is
  // considered as being in the SameDocument by the NavigationContext, so the
  // toolbar isn't shown in the DidFinishNavigation. For example this is
  // needed to load AMP pages from Google Search Result Page.
  controller_->ExitFullscreen();
}

void FullscreenWebStateObserver::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state, web_state_);
  SetWebState(nullptr);
}
