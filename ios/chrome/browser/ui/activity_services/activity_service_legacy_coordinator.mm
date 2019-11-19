// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_legacy_coordinator.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_controller.h"
#import "ios/chrome/browser/ui/activity_services/canonical_url_retriever.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_password.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/activity_services/share_protocol.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data_builder.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The histogram key to report the latency between the start of the Share Page
// operation and when the UI is ready to be presented.
const char kSharePageLatencyHistogram[] = "IOS.SharePageLatency";
}  // namespace

@interface ActivityServiceLegacyCoordinator ()<ActivityServicePassword>

// The time when the Share Page operation started.
@property(nonatomic, assign) base::TimeTicks sharePageStartTime;

// Shares the current page using the |canonicalURL|.
- (void)sharePageWithCanonicalURL:(const GURL&)canonicalURL;

@end

@implementation ActivityServiceLegacyCoordinator

@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;
@synthesize tabModel = _tabModel;

@synthesize positionProvider = _positionProvider;
@synthesize presentationProvider = _presentationProvider;

@synthesize sharePageStartTime = _sharePageStartTime;

#pragma mark - Public methods

- (void)disconnect {
  self.browserState = nil;
  self.dispatcher = nil;
}

- (void)cancelShare {
  id<ShareProtocol> controller = [ActivityServiceController sharedInstance];
  [controller cancelShareAnimated:NO];
}

#pragma mark - Properties

- (void)setDispatcher:(CommandDispatcher*)dispatcher {
  if (dispatcher == self.dispatcher) {
    return;
  }

  if (self.dispatcher) {
    [self.dispatcher stopDispatchingToTarget:self];
  }

  [dispatcher startDispatchingToTarget:self forSelector:@selector(sharePage)];
  _dispatcher = dispatcher;
}

#pragma mark - Command handlers

- (void)sharePage {
  self.sharePageStartTime = base::TimeTicks::Now();
  __weak ActivityServiceLegacyCoordinator* weakSelf = self;
  activity_services::RetrieveCanonicalUrl(
      self.tabModel.webStateList->GetActiveWebState(), ^(const GURL& url) {
        [weakSelf sharePageWithCanonicalURL:url];
      });
}

#pragma mark - Providers

- (id<PasswordFormFiller>)currentPasswordFormFiller {
  web::WebState* webState = self.tabModel.webStateList->GetActiveWebState();
  return webState ? PasswordTabHelper::FromWebState(webState)
                        ->GetPasswordFormFiller()
                  : nil;
}

#pragma mark - Private Methods

- (void)sharePageWithCanonicalURL:(const GURL&)canonicalURL {
  ShareToData* data = activity_services::ShareToDataForWebState(
      self.tabModel.webStateList->GetActiveWebState(), canonicalURL);
  if (!data)
    return;

  id<ShareProtocol> controller = [ActivityServiceController sharedInstance];
  if ([controller isActive])
    return;

  if (self.sharePageStartTime != base::TimeTicks()) {
    UMA_HISTOGRAM_TIMES(kSharePageLatencyHistogram,
                        base::TimeTicks::Now() - self.sharePageStartTime);
    self.sharePageStartTime = base::TimeTicks();
  }

  [controller shareWithData:data
               browserState:self.browserState
                 dispatcher:static_cast<id<BrowserCommands, SnackbarCommands>>(
                                self.dispatcher)
           passwordProvider:self
           positionProvider:self.positionProvider
       presentationProvider:self.presentationProvider];
}

@end
