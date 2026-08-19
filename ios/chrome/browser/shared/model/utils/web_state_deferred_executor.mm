// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/web_state_deferred_executor.h"

#import "base/memory/weak_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/web/public/navigation/navigation_manager.h"

namespace {
// The maximum duration to wait for the WebState to finish loading.
constexpr base::TimeDelta kWebStateDeferredExecutionTimeout = base::Seconds(10);
}  // namespace

@implementation WebStateDeferredExecutor {
  // Observer for the web state loading.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  // Stores the callbacks to be used once the web state is loaded.
  std::unordered_map<web::WebStateID, WebStateLoadedCompletionBlock>
      _loadedCallbacks;
  // Stores the callbacks to be used once the web state is realized.
  std::unordered_map<web::WebStateID, WebStateRealizedCompletionBlock>
      _realizedCallbacks;
  // Manages observation of multiple WebStates and ensures observers are
  // correctly removed upon destruction.
  std::unique_ptr<
      base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>>
      _scopedObservations;
  // Stores timeout timers for web states being loaded.
  std::unordered_map<web::WebStateID, std::unique_ptr<base::OneShotTimer>>
      _timers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _scopedObservations = std::make_unique<base::ScopedMultiSourceObservation<
        web::WebState, web::WebStateObserver>>(_webStateObserverBridge.get());
  }

  return self;
}

- (void)ensureWebStateIsLoaded:(web::WebState*)webState
                withCompletion:(WebStateLoadedCompletionBlock)completion {
  _loadedCallbacks[webState->GetUniqueIdentifier()] = completion;
  BOOL realized = webState->IsRealized();

  if (!realized) {
    [self observeWebState:webState];
    [self forceRealizeWebState:webState];
  }

  // Ensure the web state is actually loading/loaded by triggering restoration
  // load.
  if (webState->GetNavigationManager()) {
    webState->GetNavigationManager()->LoadIfNecessary();
  }

  [self.delegate webStateDeferredExecutor:self willLoadWebState:webState];

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();

  _loadedCallbacks[webState->GetUniqueIdentifier()] =
      ^(web::WebState* innerWebState, BOOL success) {
        [weakSelf.delegate webStateDeferredExecutor:weakSelf
                                    didLoadWebState:innerWebState
                                            success:success];
        if (completion) {
          completion(innerWebState, success);
        }
      };

  if (webState->IsLoading()) {
    [self observeWebState:webState];

    web::WebStateID webStateID = webState->GetUniqueIdentifier();
    auto timer = std::make_unique<base::OneShotTimer>();
    timer->Start(FROM_HERE, kWebStateDeferredExecutionTimeout,
                 base::BindOnce(
                     [](base::WeakPtr<web::WebState> weak_web_state,
                        __weak WebStateDeferredExecutor* weak_executor) {
                       if (web::WebState* web_state = weak_web_state.get()) {
                         [weak_executor handleTimeoutForWebState:web_state];
                       }
                     },
                     weakWebState, weakSelf));
    _timers[webStateID] = std::move(timer);
    return;
  }

  // Already loaded.
  [self webStateLoaded:webState success:YES];
}

- (void)ensureWebStateIsRealized:(web::WebState*)webState
                  withCompletion:(WebStateRealizedCompletionBlock)completion {
  BOOL realized = webState->IsRealized();

  if (realized) {
    _realizedCallbacks[webState->GetUniqueIdentifier()] = completion;
    [self invokeRealizedCallbacksForWebState:webState];
    return;
  }

  [self.delegate webStateDeferredExecutor:self
                 willForceRealizeWebState:webState];

  __weak __typeof(self) weakSelf = self;
  base::WeakPtr<web::WebState> weakWebState = webState->GetWeakPtr();

  _realizedCallbacks[webState->GetUniqueIdentifier()] =
      ^(web::WebState* innerWebState) {
        [weakSelf.delegate webStateDeferredExecutor:weakSelf
                            didForceRealizeWebState:innerWebState];
        if (completion) {
          completion(innerWebState);
        }
      };
  [self observeWebState:webState];
  [self forceRealizeWebState:webState];
}

#pragma mark - Private

// Called when the load for `webState` times out. Stops observing and triggers
// the loaded callbacks with a failure status.
- (void)handleTimeoutForWebState:(web::WebState*)webState {
  [self removeObserverForWebState:webState];
  [self webStateLoaded:webState success:NO];
}

- (void)observeWebState:(web::WebState*)webState {
  if (_scopedObservations->IsObservingSource(webState)) {
    return;
  }
  _scopedObservations->AddObservation(webState);
}

- (void)removeObserverForWebState:(web::WebState*)webState {
  if (_scopedObservations->IsObservingSource(webState)) {
    _scopedObservations->RemoveObservation(webState);
  }
}

- (void)forceRealizeWebState:(web::WebState*)webState {
  web::IgnoreOverRealizationCheck();
  webState->ForceRealized();
}

- (void)webStateLoaded:(web::WebState*)webState success:(BOOL)success {
  const web::WebStateID webStateID = webState->GetUniqueIdentifier();
  _timers.erase(webStateID);
  if (auto block = _loadedCallbacks[webStateID]) {
    _loadedCallbacks.erase(webStateID);
    block(webState, success);
  }
}

- (void)invokeRealizedCallbacksForWebState:(web::WebState*)webState {
  const web::WebStateID webStateID = webState->GetUniqueIdentifier();
  if (auto block = _realizedCallbacks[webStateID]) {
    _realizedCallbacks.erase(webStateID);
    block(webState);
  }
}



#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self removeObserverForWebState:webState];
  [self webStateLoaded:webState success:success];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  if (_loadedCallbacks.contains(webState->GetUniqueIdentifier())) {
    [self removeObserverForWebState:webState];
    [self webStateLoaded:webState success:NO];
  }
}

- (void)webStateRealized:(web::WebState*)webState {
  if (!_loadedCallbacks.contains(webState->GetUniqueIdentifier())) {
    [self removeObserverForWebState:webState];
  }
  [self invokeRealizedCallbacksForWebState:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self removeObserverForWebState:webState];
  [self invokeRealizedCallbacksForWebState:webState];
  [self webStateLoaded:webState success:NO];
}

@end
