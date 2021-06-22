// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/mobileconfig_coordinator.h"

#import <SafariServices/SafariServices.h>

#include <memory>

#include "base/scoped_observation.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/mobileconfig_tab_helper.h"
#import "ios/chrome/browser/download/mobileconfig_tab_helper_delegate.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MobileConfigCoordinator () <CRWWebStateObserver,
                                       MobileConfigTabHelperDelegate,
                                       SFSafariViewControllerDelegate,
                                       WebStateListObserving> {
  // WebStateList observers.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObserver;
}

// The WebStateList being observed.
@property(nonatomic, readonly) WebStateList* webStateList;

// Coordinator used to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// SFSafariViewController used to download .mobileconfig file. When a
// mobileconfig is downloaded from a SFSafariViewController, it's directly send
// to the Settings app.
@property(nonatomic, strong) SFSafariViewController* safariViewController;

@end

@implementation MobileConfigCoordinator

- (WebStateList*)webStateList {
  return self.browser->GetWebStateList();
}

- (void)start {
  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    [self installDelegatesForWebState:webState];
  }

  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserver>>(
      _webStateListObserverBridge.get());
  _scopedWebStateListObserver->Observe(self.webStateList);
}

- (void)stop {
  _scopedWebStateListObserver.reset();

  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    [self uninstallDelegatesForWebState:webState];
  }

  self.safariViewController = nil;
  [self.alertCoordinator stop];
}

#pragma mark - Private

// Installs delegates for |webState|.
- (void)installDelegatesForWebState:(web::WebState*)webState {
  if (MobileConfigTabHelper::FromWebState(webState)) {
    MobileConfigTabHelper::FromWebState(webState)->set_delegate(self);
  }
}

// Uninstalls delegates for |webState|.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  if (MobileConfigTabHelper::FromWebState(webState)) {
    MobileConfigTabHelper::FromWebState(webState)->set_delegate(nil);
  }
}

// Presents SFSafariViewController in order to download .mobileconfig file.
- (void)presentSFSafariViewController:(NSURL*)fileURL {
  self.safariViewController =
      [[SFSafariViewController alloc] initWithURL:fileURL];
  self.safariViewController.delegate = self;
  self.safariViewController.preferredBarTintColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];

  [self.baseViewController presentViewController:self.safariViewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self installDelegatesForWebState:webState];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self uninstallDelegatesForWebState:oldWebState];
  [self installDelegatesForWebState:newWebState];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self uninstallDelegatesForWebState:webState];
}

#pragma mark - MobileConfigTabHelperDelegate

- (void)presentMobileConfigAlertFromURL:(NSURL*)fileURL {
  if (!fileURL) {
    return;
  }

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE)
                         message:
                             l10n_util::GetNSString(
                                 IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_MESSAGE)];

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                   action:nil
                                    style:UIAlertActionStyleCancel];

  __weak MobileConfigCoordinator* weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE)
                action:^{
                  [weakSelf presentSFSafariViewController:fileURL];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

#pragma mark - SFSafariViewControllerDelegate

- (void)safariViewControllerDidFinish:(SFSafariViewController*)controller {
  [self.baseViewController dismissViewControllerAnimated:true completion:nil];
}

@end
