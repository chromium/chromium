// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/discover_feed_preview/discover_feed_preview_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_preview/discover_feed_preview_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DiscoverFeedPreviewMediator () <CRWWebStateObserver>

// The current web state associated with the preview.
@property(nonatomic, assign) web::WebState* webState;

// URL of the discover feed preview.
@property(nonatomic, assign) GURL URL;

// YES if the restoration of the webState is finished.
@property(nonatomic, assign) BOOL restorationHasFinished;

@end

@implementation DiscoverFeedPreviewMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                      previewURL:(const GURL&)previewURL {
  self = [super init];
  if (self) {
    _webState = webState;
    _URL = previewURL;
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserver.get());

    _restorationHasFinished =
        !_webState->GetNavigationManager()->IsRestoreSessionInProgress();
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  // Load the preview after the restore session has been done.
  if (success && !self.restorationHasFinished &&
      !_webState->GetNavigationManager()->IsRestoreSessionInProgress()) {
    self.restorationHasFinished = YES;

    std::string referrerURL = base::GetFieldTrialParamValueByFeature(
        kEnableDiscoverFeedPreview, kDiscoverReferrerParameter);
    if (referrerURL.empty()) {
      referrerURL = kDefaultDiscoverReferrer;
    }
    web::Referrer referrer =
        web::Referrer(GURL(referrerURL), web::ReferrerPolicyDefault);

    // Load the preview page using the copied web state.
    web::NavigationManager::WebLoadParams loadParams(self.URL);
    loadParams.referrer = referrer;

    // Attempt to prevent the WebProcess from suspending. Set this before
    // triggering the preview page loads.
    _webState->SetKeepRenderProcessAlive(true);
    _webState->GetNavigationManager()->LoadURLWithParams(loadParams);
  }
  [self updateLoadingState];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateLoadingState];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self.consumer setLoadingProgressFraction:progress];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webState = nullptr;

  [self.consumer setLoadingState:NO];
}

#pragma mark - private

// Updates the consumer to match the current loading state.
- (void)updateLoadingState {
  if (!self.restorationHasFinished)
    return;
  DCHECK(self.webState);
  DCHECK(self.consumer);

  BOOL isLoading = self.webState->IsLoading();
  [self.consumer setLoadingState:isLoading];
  if (isLoading) {
    [self.consumer
        setLoadingProgressFraction:self.webState->GetLoadingProgress()];
  }
}

@end
