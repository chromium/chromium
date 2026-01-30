// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/web_state_deferred_executor.h"

#import "base/memory/weak_ptr.h"

@implementation WebStateDeferredExecutor {
  // Observer for the web state loading.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  // Stores the callbacks to be used once the web state is loaded.
  std::unordered_map<web::WebStateID, WebStateLoadedCompletionBlock>
      _loadedCallbacks;
  // Stores the callbacks to be used once the web state is realized.
  std::unordered_map<web::WebStateID, ProceduralBlock> _realizedCallbacks;
  // Temporarily stores the active observations.
  std::unordered_map<web::WebStateID, base::WeakPtr<web::WebState>>
      _activeObservations;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
  }

  return self;
}

- (void)webState:(web::WebState*)webState
    executeOnceLoaded:(WebStateLoadedCompletionBlock)completion {
  _loadedCallbacks[webState->GetUniqueIdentifier()] = completion;
  BOOL realized = webState->IsRealized();
  BOOL loading = webState->IsLoading();

  if (!realized) {
    [self observeWebState:webState];
    [self forceRealizeWebState:webState];
    return;
  }

  [self.delegate webStateDeferredExecutor:self willLoadWebState:webState];

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();

  _loadedCallbacks[webState->GetUniqueIdentifier()] = ^(BOOL success) {
    if (weakWebState) {
      [weakSelf.delegate webStateDeferredExecutor:weakSelf
                                  didLoadWebState:weakWebState.get()
                                          success:success];
    }
    return completion(success);
  };

  if (loading) {
    [self observeWebState:webState];
    return;
  }

  // Already loaded.
  [self callLoadedCompletionForID:webState->GetUniqueIdentifier() success:YES];
}

- (void)webState:(web::WebState*)webState
    executeOnceRealized:(ProceduralBlock)completion {
  BOOL realized = webState->IsRealized();

  if (realized) {
    _realizedCallbacks[webState->GetUniqueIdentifier()] = completion;
    [self callRealizedCompletionForID:webState->GetUniqueIdentifier()];
    return;
  }

  [self.delegate webStateDeferredExecutor:self
                 willForceRealizeWebState:webState];

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();

  _realizedCallbacks[webState->GetUniqueIdentifier()] = ^{
    if (weakWebState) {
      [weakSelf.delegate webStateDeferredExecutor:weakSelf
                          didForceRealizeWebState:weakWebState.get()];
    }

    if (completion) {
      completion();
    };
  };
  [self observeWebState:webState];
  [self forceRealizeWebState:webState];
}

#pragma mark - Private

- (void)observeWebState:(web::WebState*)webState {
  BOOL alreadyObserving =
      _activeObservations.find(webState->GetUniqueIdentifier()) !=
      _activeObservations.end();
  if (alreadyObserving) {
    return;
  }
  webState->AddObserver(_webStateObserverBridge.get());
  _activeObservations[webState->GetUniqueIdentifier()] = webState->GetWeakPtr();
}

- (void)removeObserverForWebState:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserverBridge.get());
  _activeObservations.erase(webState->GetUniqueIdentifier());
}

- (void)forceRealizeWebState:(web::WebState*)webState {
  web::IgnoreOverRealizationCheck();
  webState->ForceRealized();
}

- (void)callLoadedCompletionForID:(web::WebStateID)webStateID
                          success:(BOOL)success {
  if (auto block = _loadedCallbacks[webStateID]) {
    block(success);
    _loadedCallbacks.erase(webStateID);
  }
}

- (void)callRealizedCompletionForID:(web::WebStateID)webStateID {
  if (auto block = _realizedCallbacks[webStateID]) {
    block();
    _realizedCallbacks.erase(webStateID);
  }
}

- (void)removeRemainingWebStateObservations {
  std::vector<base::WeakPtr<web::WebState>> remainingObservedWebStates;
  remainingObservedWebStates.reserve(_activeObservations.size());

  for (auto kv : _activeObservations) {
    remainingObservedWebStates.push_back(kv.second);
  }

  for (base::WeakPtr<web::WebState> weakWebState : remainingObservedWebStates) {
    web::WebState* webState = weakWebState.get();
    if (webState) {
      [self removeObserverForWebState:webState];
    }
  }
}

- (void)dealloc {
  [self removeRemainingWebStateObservations];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self removeObserverForWebState:webState];
  [self callLoadedCompletionForID:webState->GetUniqueIdentifier()
                          success:success];
}

- (void)webStateRealized:(web::WebState*)webState {
  if (!_loadedCallbacks.contains(webState->GetUniqueIdentifier())) {
    [self removeObserverForWebState:webState];
  }
  [self callRealizedCompletionForID:webState->GetUniqueIdentifier()];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self removeObserverForWebState:webState];
  [self callRealizedCompletionForID:webState->GetUniqueIdentifier()];
  [self callLoadedCompletionForID:webState->GetUniqueIdentifier() success:NO];
}

@end
