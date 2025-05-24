// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_mediator.h"

#import "base/memory/ptr_util.h"
#import "base/metrics/field_trial_params.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_consumer.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface LinkPreviewMediator () <CRWWebStateObserver>

// The current web state associated with the preview.
@property(nonatomic, assign) web::WebState* webState;

// URL of the preview.
@property(nonatomic, assign) GURL URL;

// YES if the restoration of the webState is finished.
@property(nonatomic, assign) BOOL restorationHasFinished;

// The referrer for the preview.
@property(nonatomic, assign) web::Referrer referrer;

@end

@implementation LinkPreviewMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                      previewURL:(const GURL&)previewURL
                        referrer:(const web::Referrer&)referrer {
  self = [super init];
  if (self) {
    _webState = webState;
    _URL = previewURL;
    _referrer = referrer;
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserver.get());
    _restorationHasFinished = NO;
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
  if (success && !self.restorationHasFinished) {
    self.restorationHasFinished = YES;

    // Load the preview page using the copied web state.
    web::NavigationManager::WebLoadParams loadParams(self.URL);
    loadParams.referrer = self.referrer;

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

- (void)webState:(web::WebState*)webState
    didRedirectNavigation:(web::NavigationContext*)navigation_context {
  GURL redirectURL = navigation_context->GetUrl();
  NSString* redirectOrigin = base::SysUTF16ToNSString(
      url_formatter::FormatUrl(redirectURL.DeprecatedGetOriginAsURL()));
  if (base::SysUTF16ToNSString(url_formatter::FormatUrl(
          self.URL.DeprecatedGetOriginAsURL())) != redirectOrigin) {
    [self updateOrigin:redirectOrigin];
  }
  self.URL = redirectURL;
}

#pragma mark - private

// Updates the consumer to match the current loading state.
- (void)updateLoadingState {
  if (!self.restorationHasFinished) {
    return;
  }

  if (!self.consumer) {
    return;
  }
  DCHECK(self.webState);

  BOOL isLoading = self.webState->IsLoading();
  [self.consumer setLoadingState:isLoading];
  if (isLoading) {
    [self.consumer
        setLoadingProgressFraction:self.webState->GetLoadingProgress()];
  }
}

// Updates the consumer to show the current origin.
- (void)updateOrigin:(NSString*)origin {
  [self.consumer setPreviewOrigin:origin];
}

@end
