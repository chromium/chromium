// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_iph_coordinator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_mediator.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_iph_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/web/public/download/download_task.h"
#import "ui/base/device_form_factor.h"

@implementation AutoDeletionIPHCoordinator {
  // The ViewController for the Auto-deletion IPH.
  AutoDeletionIPHViewController* _viewController;
  // The mediator for auto-deletion.
  AutoDeletionMediator* _mediator;
  // The task that is downloading the content to the device.
  raw_ptr<web::DownloadTask> _downloadTask;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                              downloadTask:(web::DownloadTask*)task {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _downloadTask = task;
  }

  return self;
}

- (void)start {
  _viewController =
      [[AutoDeletionIPHViewController alloc] initWithBrowser:self.browser];
  PrefService* localState = GetApplicationContext()->GetLocalState();
  _mediator = [[AutoDeletionMediator alloc] initWithLocalState:localState
                                                       browser:self.browser
                                                  downloadTask:_downloadTask];
  _viewController.mutator = _mediator;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nullptr;
}

@end
