// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/security/web_interstitial_impl.h"

#import <WebKit/WebKit.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/common/crw_web_view_content_view.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/security/web_interstitial_delegate.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The delegate of the web view that is used to display the HTML content.
// It intercepts JavaScript-triggered commands and forwards them
// to the interstitial.
@interface CRWWebInterstitialImplWKWebViewDelegate
    : NSObject <WKNavigationDelegate>
// Initializes a CRWWebInterstitialImplWKWebViewDelegate which will
// forward JavaScript commands from its WKWebView to |interstitial|.
- (instancetype)initWithInterstitial:(web::WebInterstitialImpl*)interstitial;
@end

@implementation CRWWebInterstitialImplWKWebViewDelegate {
  web::WebInterstitialImpl* _interstitial;
}

- (instancetype)initWithInterstitial:(web::WebInterstitialImpl*)interstitial {
  self = [super init];
  if (self)
    _interstitial = interstitial;
  return self;
}

- (BOOL)shouldStartLoadWithRequest:(NSURLRequest*)request {
  NSString* requestString = request.URL.absoluteString;
  // If the request is a JavaScript-triggered command, parse it and forward the
  // command to |interstitial_|.
  NSString* const kCommandPrefix = @"js-command:";
  if ([requestString hasPrefix:kCommandPrefix]) {
    DCHECK(_interstitial);
    _interstitial->CommandReceivedFromWebView(
        [requestString substringFromIndex:kCommandPrefix.length]);
    return NO;
  }
  return YES;
}

#pragma mark -
#pragma mark WKNavigationDelegate methods

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)navigationAction
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  decisionHandler([self shouldStartLoadWithRequest:navigationAction.request]
                      ? WKNavigationActionPolicyAllow
                      : WKNavigationActionPolicyCancel);
}

@end

namespace web {

// static
WebInterstitial* WebInterstitial::CreateInterstitial(
    WebState* web_state,
    bool new_navigation,
    const GURL& url,
    std::unique_ptr<WebInterstitialDelegate> delegate) {
  WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state);
  return new WebInterstitialImpl(web_state_impl, new_navigation, url,
                                 std::move(delegate));
}

WebInterstitialImpl::WebInterstitialImpl(
    WebStateImpl* web_state,
    bool new_navigation,
    const GURL& url,
    std::unique_ptr<WebInterstitialDelegate> delegate)
    : web_state_(web_state),
      navigation_manager_(&web_state->GetNavigationManagerImpl()),
      url_(url),
      new_navigation_(new_navigation),
      action_taken_(false),
      delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  web_state_->AddObserver(this);
}

WebInterstitialImpl::~WebInterstitialImpl() {
  Hide();
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

CRWContentView* WebInterstitialImpl::GetContentView() const {
  return content_view_;
}

const GURL& WebInterstitialImpl::GetUrl() const {
  return url_;
}

void WebInterstitialImpl::Show() {
  if (!content_view_) {
    web_view_delegate_ = [[CRWWebInterstitialImplWKWebViewDelegate alloc]
        initWithInterstitial:this];
    web_view_ = web::BuildWKWebView(CGRectZero, web_state_->GetBrowserState());
    [web_view_ setNavigationDelegate:web_view_delegate_];
    [web_view_ setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                    UIViewAutoresizingFlexibleHeight)];
    NSString* html = base::SysUTF8ToNSString(delegate_->GetHtmlContents());
    [web_view_ loadHTMLString:html baseURL:net::NSURLWithGURL(GetUrl())];
    content_view_ =
        [[CRWWebViewContentView alloc] initWithWebView:web_view_
                                            scrollView:[web_view_ scrollView]];
  }

  web_state_->ShowWebInterstitial(this);

  if (new_navigation_) {
    // TODO(crbug.com/706578): Plumb transient entry handling through
    // NavigationManager, and remove the NavigationManagerImpl usage here.
    navigation_manager_->AddTransientItem(url_);

    // Give delegates a chance to set some states on the navigation item.
    delegate_->OverrideItem(navigation_manager_->GetTransientItem());

    web_state_->DidChangeVisibleSecurityState();
  }
}

void WebInterstitialImpl::Hide() {
  web_state_->ClearTransientContent();
}

void WebInterstitialImpl::DontProceed() {
  // Proceed() and DontProceed() are not re-entrant, as they delete |this|.
  if (action_taken_)
    return;
  action_taken_ = true;

  // Clear the pending entry, since that's the page that's not being
  // proceeded to.
  web_state_->GetNavigationManager()->DiscardNonCommittedItems();

  Hide();

  delegate_->OnDontProceed();

  delete this;
}

void WebInterstitialImpl::Proceed() {
  // Proceed() and DontProceed() are not re-entrant, as they delete |this|.
  if (action_taken_)
    return;
  action_taken_ = true;
  Hide();
  delegate_->OnProceed();
  delete this;
}

void WebInterstitialImpl::WebStateDestroyed(WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  // There is no need to remove the current instance from WebState's observer
  // as DontProceed() delete "this" and the removal is done in the destructor.
  // In addition since the current instance has been deleted, "this" should no
  // longer be used after the method call.
  DontProceed();
}

void WebInterstitialImpl::CommandReceivedFromWebView(NSString* command) {
  delegate_->CommandReceived(base::SysNSStringToUTF8(command));
}

void WebInterstitialImpl::ExecuteJavaScript(
    NSString* script,
    void (^completion_handler)(id, NSError*)) {
  web::ExecuteJavaScript(web_view_, script, completion_handler);
}

}  // namespace web
