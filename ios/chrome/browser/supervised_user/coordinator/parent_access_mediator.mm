// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator.h"

#import <memory>

#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "url/gurl.h"

@implementation ParentAccessMediator {
  std::unique_ptr<web::WebState> _webState;
}

- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState {
  if ((self = [super init])) {
    CHECK(webState);
    _webState = std::move(webState);
  }
  return self;
}

- (void)setConsumer:(id<ParentAccessConsumer>)consumer {
  _consumer = consumer;

  _webState->SetWebUsageEnabled(true);
  web::NavigationManager::WebLoadParams webParams =
      web::NavigationManager::WebLoadParams(
          supervised_user::GetParentAccessURLForIOS());
  _webState->GetNavigationManager()->LoadURLWithParams(webParams);
  // TODO(crbug.com/41407753): For a newly created WebState, the session
  // will not be restored until LoadIfNecessary call. Remove when fixed.
  _webState->GetNavigationManager()->LoadIfNecessary();

  [_consumer setWebView:_webState->GetView()];
}

- (void)disconnect {
  _webState.reset();
}

@end
