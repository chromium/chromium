// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_observer.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_proxy_observer.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/url_util.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using fullscreen::features::ViewportAdjustmentExperiment;

namespace {
// Returns whether fullscreen should be disabled for |web_state|'s SSL status.
// This will return true if the visible NavigationItem's SSL has a broken
// security style or is showing mixed content.  If the UI refresh is enabled,
// fullscreen does not need to be disabled for certificate issues, as the
// omnibox security indicator is never fully hidden in fullscreen mode.
bool ShouldDisableFullscreenForWebStateSSL(web::WebState* web_state) {
  if (IsUIRefreshPhase1Enabled())
    return false;
  if (!web_state)
    return false;
  web::NavigationManager* manager = web_state->GetNavigationManager();
  if (!manager)
    return false;
  const web::NavigationItem* item = manager->GetVisibleItem();
  if (!item)
    return false;
  const web::SSLStatus& ssl = item->GetSSL();
  return ssl.security_style == web::SECURITY_STYLE_AUTHENTICATION_BROKEN ||
         (ssl.content_status & web::SSLStatus::DISPLAYED_INSECURE_CONTENT) > 0;
}
}  // namespace

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
  // Update the model according to the new WebState.
  SetIsLoading(web_state_ ? web_state->IsLoading() : false);
  SetDisableFullscreenForSSL(ShouldDisableFullscreenForWebStateSSL(web_state_));
  // Update the scroll view replacement handler's proxy.
  web_view_proxy_observer_.proxy =
      web_state_ ? web_state_->GetWebViewProxy() : nil;
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
  // - PDFs need to be inset using the scroll view's |contentInset| property or
  //   the floating page indicator is laid out incorrectly.
  // - For normal pages, using |contentInset| breaks the layout of fixed-
  //   position DOM elements, so top padding must be accomplished by updating
  //   the WKWebView's frame.
  bool force_content_inset =
      fullscreen::features::GetActiveViewportExperiment() ==
      ViewportAdjustmentExperiment::CONTENT_INSET;
  web_state->GetWebViewProxy().shouldUseViewContentInset =
      force_content_inset ||
      web_state->GetContentsMimeType() == "application/pdf";
  // Only reset the model for document-changing navigations or same-document
  // navigations that update the visible URL.
  if (!navigation_context->IsSameDocument() || url_changed)
    model_->ResetForNavigation();
  // Disable fullscreen if there is a problem with the SSL status.
  SetDisableFullscreenForSSL(ShouldDisableFullscreenForWebStateSSL(web_state));
}

void FullscreenWebStateObserver::DidStartLoading(web::WebState* web_state) {
  SetIsLoading(true);
  if (IsUIRefreshPhase1Enabled()) {
    // This is done to show the toolbar when navigating to a page that is
    // considered as being in the SameDocument by the NavigationContext, so the
    // toolbar isn't shown in the DidFinishNavigation. For example this is
    // needed to load AMP pages from Google Search Result Page.
    controller_->ExitFullscreen();
  }
}

void FullscreenWebStateObserver::DidStopLoading(web::WebState* web_state) {
  SetIsLoading(false);
}

void FullscreenWebStateObserver::DidChangeVisibleSecurityState(
    web::WebState* web_state) {
  SetDisableFullscreenForSSL(ShouldDisableFullscreenForWebStateSSL(web_state));
}

void FullscreenWebStateObserver::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state, web_state_);
  SetWebState(nullptr);
}

void FullscreenWebStateObserver::SetDisableFullscreenForSSL(bool disable) {
  if (!!ssl_disabler_.get() == disable)
    return;
  ssl_disabler_ = disable
                      ? std::make_unique<ScopedFullscreenDisabler>(controller_)
                      : nullptr;
}

void FullscreenWebStateObserver::SetIsLoading(bool loading) {
  if (IsUIRefreshPhase1Enabled()) {
    if (loading)
      controller_->ExitFullscreen();
  } else {
    loading_disabler_ =
        loading ? std::make_unique<ScopedFullscreenDisabler>(controller_)
                : nullptr;
  }
}
