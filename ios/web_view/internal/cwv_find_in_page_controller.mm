// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_find_in_page_controller_internal.h"

#import "ios/web/public/find_in_page/find_in_page_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface CWVFindInPageController () <CRWWebStateObserver>
@end

@implementation CWVFindInPageController {
  // Object that manages searches and match traversals.
  web::FindInPageManager* _findInPageManager;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  base::WeakPtr<web::WebState> _webState;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    DCHECK(webState);
    DCHECK(webState->IsRealized());

    // The Find in Page manager should not be attached yet.
    DCHECK(!web::FindInPageManager::FromWebState(webState));
    web::FindInPageManager::CreateForWebState(webState);
    _findInPageManager = web::FindInPageManager::FromWebState(webState);

    _webState = webState->GetWeakPtr();
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

- (void)startFindInPage {
  _findInPageManager->Find(@"", web::FindInPageOptions::FindInPageSearch);
}

- (void)findStringInPage:(NSString*)query {
  _findInPageManager->Find(query, web::FindInPageOptions::FindInPageSearch);
}

- (void)findNextStringInPage {
  _findInPageManager->Find(nil, web::FindInPageOptions::FindInPageNext);
}

- (void)findPreviousStringInPage {
  _findInPageManager->Find(nil, web::FindInPageOptions::FindInPagePrevious);
}

- (void)stopFindInPage {
  _findInPageManager->StopFinding();
}

- (BOOL)canFindInPage {
  return _findInPageManager->CanSearchContent();
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState.get(), webState);
  _findInPageManager = nullptr;

  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge.reset();
  _webState = nullptr;
}

@end
