// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/ar_quick_look_coordinator.h"

#import <QuickLook/QuickLook.h>

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

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
                                      QLPreviewControllerDataSource,
                                      QLPreviewControllerDelegate> {
  // WebStateList observers.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserver>>
      _scopedWebStateListObserver;
}

// The WebStateList being observed.
@property(nonatomic, assign) WebStateList* webStateList;

// Whether the coordinator is started.
@property(nonatomic, assign) BOOL started;

// The file URL pointing to the downloaded USDZ format file.
@property(nonatomic, copy) NSURL* fileURL;

// Displays USDZ format files. Set as a weak reference so it only exists while
// its being presented by baseViewController.
@property(nonatomic, weak) QLPreviewController* viewController;

@end

@implementation ARQuickLookCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  DCHECK(webStateList);
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _webStateList = webStateList;
  }
  return self;
}

- (void)start {
  if (self.started)
    return;

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

#pragma mark - Private

// Adds observer for WebStateList.
- (void)addWebStateListObserver {
  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver =
      std::make_unique<ScopedObserver<WebStateList, WebStateListObserver>>(
          _webStateListObserverBridge.get());
  _scopedWebStateListObserver->Add(self.webStateList);
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
    didFinishDowloadingFileWithURL:(NSURL*)fileURL {
  self.fileURL = fileURL;

  UMA_HISTOGRAM_ENUMERATION(
      kIOSPresentQLPreviewControllerHistogram,
      GetHistogramEnum(self.baseViewController, self.fileURL));

  // QLPreviewController should not be presented if the file URL is nil.
  if (!self.fileURL) {
    return;
  }

  QLPreviewController* viewController = [[QLPreviewController alloc] init];
  viewController.dataSource = self;
  viewController.delegate = self;
  [self.baseViewController presentViewController:viewController
                                        animated:YES
                                      completion:nil];
  self.viewController = viewController;
}

#pragma mark - QLPreviewControllerDataSource

- (NSInteger)numberOfPreviewItemsInPreviewController:
    (QLPreviewController*)controller {
  return 1;
}

- (id<QLPreviewItem>)previewController:(QLPreviewController*)controller
                    previewItemAtIndex:(NSInteger)index {
  return self.fileURL;
}

#pragma mark - QLPreviewControllerDelegate

- (void)previewControllerDidDismiss:(QLPreviewController*)controller {
  self.viewController = nil;
  self.fileURL = nil;
}

@end
