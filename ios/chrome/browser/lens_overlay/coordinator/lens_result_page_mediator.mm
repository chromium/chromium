// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"

#import <memory>

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

/// Returns whether the navigation is allowed inside of the result page.
BOOL IsValidURLToOpenInResultsPage(const GURL& URL) {
  std::string_view host = URL.host_piece();
  return base::EqualsCaseInsensitiveASCII(host, "google.com") ||
         base::EqualsCaseInsensitiveASCII(host, "www.google.com");
}

}  // namespace

@interface LensResultPageMediator () <CRWWebStatePolicyDecider>

@end

@implementation LensResultPageMediator {
  /// WebState for lens results.
  std::unique_ptr<web::WebState> _webState;
  /// WebState delegate from the browser.
  web::WebStateDelegate* _browserWebStateDelegate;
  /// Web state policy decider.
  std::unique_ptr<web::WebStatePolicyDeciderBridge> _policyDeciderBridge;
  /// Whether the browser is off the record.
  BOOL _isIncognito;
}

- (instancetype)
     initWithWebStateParams:(const web::WebState::CreateParams&)params
    browserWebStateDelegate:(web::WebStateDelegate*)browserWebStateDelegate
                isIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _webState = web::WebState::Create(params);
    // TODO(crbug.com/349100642): Create a custom WebStateDelegate to present a
    // custom context menu.
    _browserWebStateDelegate = browserWebStateDelegate;
    _webState->SetDelegate(_browserWebStateDelegate);
    AttachTabHelpers(_webState.get(), NO);
    _policyDeciderBridge = std::make_unique<web::WebStatePolicyDeciderBridge>(
        _webState.get(), self);
    _isIncognito = isIncognito;
  }
  return self;
}

- (void)setConsumer:(id<LensResultPageConsumer>)consumer {
  _consumer = consumer;
  CHECK(_webState, kLensOverlayNotFatalUntil);
  _webState->SetWebUsageEnabled(true);
  [self.consumer setWebView:_webState->GetView()];
}

- (void)disconnect {
  _policyDeciderBridge.reset();
  _webState.reset();
}

#pragma mark - LensOverlayResultConsumer

- (void)loadResultsURL:(GURL)URL {
  CHECK(_webState, kLensOverlayNotFatalUntil);

  _webState->OpenURL(web::WebState::OpenURLParams(
      URL, web::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
}

#pragma mark - CRWWebStatePolicyDecider

- (void)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(web::WebStatePolicyDecider::RequestInfo)requestInfo
           decisionHandler:(PolicyDecisionHandler)decisionHandler {
  GURL URL = net::GURLWithNSURL(request.URL);
  if (requestInfo.target_frame_is_main && !IsValidURLToOpenInResultsPage(URL)) {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Cancel());
    OpenNewTabCommand* command =
        [[OpenNewTabCommand alloc] initWithURL:URL
                                      referrer:web::Referrer()
                                   inIncognito:_isIncognito
                                  inBackground:NO
                                      appendTo:OpenPosition::kCurrentTab];
    [self.applicationHandler openURLInNewTab:command];
  } else {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
  }
}

@end
