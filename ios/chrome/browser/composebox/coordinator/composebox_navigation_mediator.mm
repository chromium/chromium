// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_navigation_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "url/gurl.h"

@implementation ComposeboxNavigationMediator {
  // The URL loading browser agent.
  raw_ptr<UrlLoadingBrowserAgent> _urlLoadingBrowserAgent;
}

- (instancetype)initWithUrlLoadingBrowserAgent:
    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent {
  self = [super init];
  if (self) {
    _urlLoadingBrowserAgent = urlLoadingBrowserAgent;
  }
  return self;
}

- (void)disconnect {
  _urlLoadingBrowserAgent = nullptr;
}

#pragma mark - ComposeboxURLLoader

- (void)prepareLoadWithClientToAimMessage:
    (const lens::ClientToAimMessage&)message {
  // NO-OP
}

- (void)loadURLParams:(const UrlLoadParams&)URLLoadParams {
  if (URLLoadParams.web_params.url.SchemeIs(url::kJavaScriptScheme)) {
    [self.delegate navigationMediator:self
             wantsToLoadJavaScriptURL:URLLoadParams.web_params.url];
  } else {
    _urlLoadingBrowserAgent->Load(URLLoadParams);
  }
  [self dismissComposebox];
}

#pragma mark - Private

// Asks the delegate to dismiss the composebox.
- (void)dismissComposebox {
  [self.delegate navigationMediatorDidFinish:self];
}

@end
