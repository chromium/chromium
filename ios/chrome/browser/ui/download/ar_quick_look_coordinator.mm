// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/ar_quick_look_coordinator.h"

#import <ARKit/ARKit.h>
#import <QuickLook/QuickLook.h>

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper_delegate.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kIOSPresentQLPreviewControllerHistogram[] =
    "Download.IOSPresentQLPreviewControllerResult";

namespace {

// Returns an enum for Download.IOSPresentQLPreviewControllerResult.
PresentQLPreviewController GetHistogramEnum(
    UIViewController* base_view_controller,
    bool is_file_valid) {
  if (!is_file_valid) {
    return PresentQLPreviewController::kInvalidFile;
  }

  if (!base_view_controller.presentedViewController) {
    return PresentQLPreviewController::kSuccessful;
  }

  if ([base_view_controller.presentedViewController
          isKindOfClass:[QLPreviewController class]]) {
    return PresentQLPreviewController::kAnotherQLPreviewControllerIsPresented;
  }

  return PresentQLPreviewController::kAnotherViewControllerIsPresented;
}

}  // namespace

@interface ARQuickLookCoordinator () <WebStateListObserving,
                                      ARQuickLookTabHelperDelegate,
                                      CRWWebStateObserver,
                                      QLPreviewControllerDataSource,
                                      QLPreviewControllerDelegate> {
  // WebStateList observers.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObserver;
  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
}

// The WebStateList being observed.
@property(nonatomic, readonly) WebStateList* webStateList;

// Whether the coordinator is started.
@property(nonatomic, assign) BOOL started;

// The file URL pointing to the downloaded USDZ format file.
@property(nonatomic, copy) NSURL* fileURL;

// Displays USDZ format files. Set as a weak reference so it only exists while
// its being presented by baseViewController.
@property(nonatomic, weak) QLPreviewController* viewController;

@property(nonatomic, assign) web::WebState* webState;

@property(nonatomic, assign) BOOL allowsContentScaling;

@end

@implementation ARQuickLookCoordinator

- (WebStateList*)webStateList {
  return self.browser->GetWebStateList();
}

- (void)start {
  if (self.started)
    return;

  _webStateObserverBridge = std::make_unique<web::WebStateObserverBridge>(self);

  // Install delegates for each WebState in WebStateList.
  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    [self installDelegatesForWebState:webState];
  }

  [self addWebStateListObserver];
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;

  [self removeWebStateListObserver];
  self.webState = nullptr;

  // Uninstall delegates for each WebState in WebStateList.
  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    [self uninstallDelegatesForWebState:webState];
  }

  [self.viewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;
  self.fileURL = nil;
  self.started = NO;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - Private

// Adds observer for WebStateList.
- (void)addWebStateListObserver {
  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserver>>(
      _webStateListObserverBridge.get());
  _scopedWebStateListObserver->Observe(self.webStateList);
}

// Removes observer for WebStateList.
- (void)removeWebStateListObserver {
  _scopedWebStateListObserver.reset();
  _webStateListObserverBridge.reset();
}

// Installs delegates for |webState|.
- (void)installDelegatesForWebState:(web::WebState*)webState {
  if (ARQuickLookTabHelper::FromWebState(webState)) {
    ARQuickLookTabHelper::FromWebState(webState)->set_delegate(self);
  }
}

// Uninstalls delegates for |webState|.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  if (ARQuickLookTabHelper::FromWebState(webState)) {
    ARQuickLookTabHelper::FromWebState(webState)->set_delegate(nil);
  }
}

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  if (_webState)
    _webState->RemoveObserver(_webStateObserverBridge.get());
  _webState = webState;
  if (_webState) {
    _webState->AddObserver(_webStateObserverBridge.get());
  }
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

#pragma mark - ARQuickLookTabHelperDelegate

- (void)ARQuickLookTabHelper:(ARQuickLookTabHelper*)tabHelper
    didFinishDowloadingFileWithURL:(NSURL*)fileURL
              allowsContentScaling:(BOOL)allowsScaling {
  self.fileURL = fileURL;
  self.allowsContentScaling = allowsScaling;

  base::UmaHistogramEnumeration(
      kIOSPresentQLPreviewControllerHistogram,
      GetHistogramEnum(self.baseViewController, self.fileURL));

  // QLPreviewController should not be presented if the file URL is nil.
  if (!self.fileURL) {
    return;
  }

  QLPreviewController* viewController = [[QLPreviewController alloc] init];
  viewController.dataSource = self;
  viewController.delegate = self;
  self.webState = tabHelper->web_state();
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController
      presentViewController:viewController
                   animated:YES
                 completion:^{
                   if (weakSelf.webState)
                     weakSelf.webState->DidCoverWebContent();
                 }];
  self.viewController = viewController;
}

#pragma mark - QLPreviewControllerDataSource

- (NSInteger)numberOfPreviewItemsInPreviewController:
    (QLPreviewController*)controller {
  return 1;
}

- (id<QLPreviewItem>)previewController:(QLPreviewController*)controller
                    previewItemAtIndex:(NSInteger)index {
  if (@available(iOS 13, *)) {
    ARQuickLookPreviewItem* item =
        [[ARQuickLookPreviewItem alloc] initWithFileAtURL:self.fileURL];
    item.allowsContentScaling = self.allowsContentScaling;
    return item;
  }
  return self.fileURL;
}

#pragma mark - QLPreviewControllerDelegate

- (void)previewControllerDidDismiss:(QLPreviewController*)controller {
  if (self.webState)
    self.webState->DidRevealWebContent();
  self.webState = nullptr;
  self.viewController = nil;
  self.fileURL = nil;
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  self.webState = nullptr;
}

@end
