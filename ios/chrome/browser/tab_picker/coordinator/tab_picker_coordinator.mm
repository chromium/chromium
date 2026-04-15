// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_logger.h"
#import "ios/chrome/browser/tab_picker/ui/tab_picker_view_controller.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/ui/base_grid_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"

@interface TabPickerCoordinator ()

// Returns `YES` if the coordinator is started.
@property(nonatomic, assign) BOOL started;

@end

@implementation TabPickerCoordinator {
  /// The tab picker mediator.
  TabPickerMediator* _mediator;
  /// The tab picker view controller.
  TabPickerViewController* _viewController;
  // The navigation controller displaying the tab picker.
  UINavigationController* _navigationController;
}

- (void)start {
  _viewController = [[TabPickerViewController alloc] init];

  _mediator = [[TabPickerMediator alloc]
        initWithGridConsumer:_viewController.gridViewController
           tabPickerConsumer:_viewController
      tabsAttachmentDelegate:self];
  _mediator.logger = self.logger;
  _mediator.browser = self.browser;

  _viewController.mutator = _mediator;
  _viewController.gridViewController.snapshotAndfaviconDataSource = _mediator;
  _viewController.gridViewController.mutator = _mediator;
  _viewController.gridViewController.gridProvider = _mediator;
  _viewController.tabPickerHandler = self.tabPickerHandler;

  if (self.browser->GetProfile()->IsOffTheRecord()) {
    _viewController.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
  self.started = YES;
  if ([self.logger respondsToSelector:@selector(logTabPickerShown)]) {
    [self.logger logTabPickerShown];
  }
}

- (void)stop {
  if (!self.started) {
    return;
  }
  if ([self.logger respondsToSelector:@selector(logTabPickerHidden)]) {
    [self.logger logTabPickerHidden];
  }
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  _navigationController = nil;
  [super stop];
}

#pragma mark - TabsAttachmentDelegate

- (void)attachSelectedTabs:(TabPickerMediator*)tabPickerMediator
       selectedWebStateIDs:(std::set<web::WebStateID>)selectedWebStateIDs
         cachedWebStateIDs:(std::set<web::WebStateID>)cachedWebStateIDs {
  [self.delegate attachSelectedTabsWithWebStateIDs:selectedWebStateIDs
                                 cachedWebStateIDs:cachedWebStateIDs];
}

- (std::set<web::WebStateID>)preselectedWebStateIDs {
  return [self.delegate attachedWebStateIDsInCurrentContext];
}

- (NSUInteger)maxTabAttachmentCount {
  return [self.delegate maxTabAttachmentCount];
}

@end
