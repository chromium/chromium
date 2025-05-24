// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator.h"

#import <memory>

#import "base/timer/timer.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator_delegate.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_consumer.h"
#import "ios/components/ui_util/dynamic_type_util.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "url/gurl.h"

@interface ParentAccessMediator () <CRWWebStateObserver>
@end

@implementation ParentAccessMediator {
  // URL for the PACP widget.
  GURL _parentAccessURL;
  // WebState to load the PACP widget.
  std::unique_ptr<web::WebState> _webState;
  // Observer for the WebState.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  // Configurable timer to dismiss the parent access bottom sheet if the
  // WebState is unresponsive.
  base::OneShotTimer _initialLoadTimer;
}

- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                 parentAccessURL:(const GURL&)parentAccessURL {
  if ((self = [super init])) {
    _parentAccessURL = parentAccessURL;
    CHECK(webState);
    _webState = std::move(webState);
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
  }
  return self;
}

- (void)setConsumer:(id<ParentAccessConsumer>)consumer {
  _consumer = consumer;

  _webState->SetWebUsageEnabled(true);
  web::NavigationManager::WebLoadParams webParams =
      web::NavigationManager::WebLoadParams(_parentAccessURL);
  _webState->GetNavigationManager()->LoadURLWithParams(webParams);
  // TODO(crbug.com/41407753): For a newly created WebState, the session
  // will not be restored until LoadIfNecessary call. Remove when fixed.
  _webState->GetNavigationManager()->LoadIfNecessary();
  // Hide the WebView initially as it does not adapt to dark mode styling.
  [_consumer setWebViewHidden:YES];
  [_consumer setWebView:_webState->GetView()];

  // Set a timer to dismiss the bottom sheet if the web state fails to load in
  // time. This prevents the sheet from hanging indefinitely if there's a
  // problem loading the widget.
  __weak __typeof(self) weakSelf = self;
  _initialLoadTimer.Start(
      FROM_HERE,
      base::Milliseconds(
          supervised_user::kLocalWebApprovalBottomSheetLoadTimeoutMs.Get()),
      base::BindOnce(^{
        [weakSelf.delegate hideParentAccessBottomSheetOnTimeout];
      }));
}

- (void)disconnect {
  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge.reset();
  _webState.reset();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  // Stop the timer that would dismiss the unresponsive bottom sheet.
  _initialLoadTimer.Stop();

  // Adjust text zoom for the PACP web state to respect the system font size
  // preference. The multiplier is converted to a percentage (e.g., 1.5x becomes
  // 150%).
  CHECK(webState->ContentIsHTML());
  ios::provider::SetTextZoomForWebState(
      webState, 100. * ui_util::SystemSuggestedFontSizeMultiplier());

  // Unhide the WebView as it should be loaded with correct styling.
  [_consumer setWebViewHidden:NO];
}

@end
