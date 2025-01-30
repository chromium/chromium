// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator.h"

#import <memory>

#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_consumer.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "url/gurl.h"

@interface ParentAccessMediator () <CRWWebStateObserver>
@end

@implementation ParentAccessMediator {
  std::unique_ptr<web::WebState> _webState;

  // Used to observe the active WebState.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
}

- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState {
  if ((self = [super init])) {
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
      web::NavigationManager::WebLoadParams(
          supervised_user::GetParentAccessURLForIOS(
              GetApplicationContext()->GetApplicationLocale()));
  _webState->GetNavigationManager()->LoadURLWithParams(webParams);
  // TODO(crbug.com/41407753): For a newly created WebState, the session
  // will not be restored until LoadIfNecessary call. Remove when fixed.
  _webState->GetNavigationManager()->LoadIfNecessary();
  // Hide the WebView initially as it does not adapt to dark mode styling.
  [_consumer setWebViewHidden:YES];
  [_consumer setWebView:_webState->GetView()];
}

- (void)disconnect {
  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge.reset();
  _webState.reset();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  // Unhide the WebView once it is loaded with correct styling.
  [_consumer setWebViewHidden:NO];
}

@end
