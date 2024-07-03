// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"

#import <memory>

#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/web/public/web_state.h"

@implementation LensResultPageMediator {
  /// WebState for lens results.
  std::unique_ptr<web::WebState> _webState;
  /// WebState delegate from the browser.
  web::WebStateDelegate* _browserWebStateDelegate;
}

- (instancetype)
     initWithWebStateParams:(const web::WebState::CreateParams&)params
    browserWebStateDelegate:(web::WebStateDelegate*)browserWebStateDelegate {
  self = [super init];
  if (self) {
    _webState = web::WebState::Create(params);
    // TODO(crbug.com/349100642): Create a custom WebStateDelegate to present a
    // custom context menu.
    _browserWebStateDelegate = browserWebStateDelegate;
    _webState->SetDelegate(_browserWebStateDelegate);
    AttachTabHelpers(_webState.get(), NO);
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
  _webState.reset();
}

#pragma mark - LensOverlayResultConsumer

- (void)loadResultsURL:(GURL)URL {
  CHECK(_webState, kLensOverlayNotFatalUntil);

  _webState->OpenURL(web::WebState::OpenURLParams(
      URL, web::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
}

@end
