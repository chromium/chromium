// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_NAVIGATION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_NAVIGATION_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/composebox/coordinator/composebox_url_loader.h"
#import "ios/web/public/web_state.h"

@protocol ComposeboxNavigationConsumer;
@class ComposeboxNavigationMediator;
class UrlLoadingBrowserAgent;

// Delegate for the composebox navigation mediator.
@protocol ComposeboxNavigationMediatorDelegate

// Called when the navigation mediator requires the composebox to be
// dismissed.
- (void)navigationMediatorDidFinish:
    (ComposeboxNavigationMediator*)navigationMediator;

// Informs the delegate to handle a JavaScript URL.
- (void)navigationMediator:(ComposeboxNavigationMediator*)navigationMediator
    wantsToLoadJavaScriptURL:(const GURL&)URL;

@end

// A mediator for the composebox's navigation.
@interface ComposeboxNavigationMediator : NSObject <ComposeboxURLLoader>

// The consumer for this mediator.
@property(nonatomic, weak) id<ComposeboxNavigationConsumer> consumer;

// The delegate for this mediator.
@property(nonatomic, weak) id<ComposeboxNavigationMediatorDelegate> delegate;

// Initializes the mediator.
- (instancetype)initWithUrlLoadingBrowserAgent:
                    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent
                                webStateParams:
                                    (const web::WebState::CreateParams&)params;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_NAVIGATION_MEDIATOR_H_
