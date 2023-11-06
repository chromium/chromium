// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/tab_pickup/tab_pickup_infobar_modal_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/infobars/modals/tab_pickup/infobar_tab_pickup_table_view_controller.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/tab_pickup/tab_pickup_infobar_modal_overlay_mediator.h"

@interface TabPickupInfobarModalOverlayCoordinator ()

// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, strong, readwrite)
    TabPickupInfobarModalOverlayMediator* modalMediator;
@property(nonatomic, strong, readwrite)
    InfobarTabPickupTableViewController* modalViewController;

@end

@implementation TabPickupInfobarModalOverlayCoordinator

#pragma mark - Public

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

@end

@implementation TabPickupInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  DCHECK(!self.modalMediator);
  DCHECK(!self.modalViewController);

  InfobarTabPickupTableViewController* modalViewController =
      [[InfobarTabPickupTableViewController alloc] init];

  TabPickupInfobarModalOverlayMediator* modalMediator =
      [[TabPickupInfobarModalOverlayMediator alloc]
          initWithUserLocalPrefService:GetApplicationContext()->GetLocalState()
                           syncService:SyncServiceFactory::GetForBrowserState(
                                           self.browser->GetBrowserState())
                              consumer:modalViewController
                               request:self.request];
  modalViewController.delegate = modalMediator;
  self.modalMediator = modalMediator;
  self.modalViewController = modalViewController;
}

- (void)resetModal {
  DCHECK(self.modalMediator);
  DCHECK(self.modalViewController);

  self.modalViewController.delegate = nil;
  self.modalViewController = nil;

  [self.modalMediator disconnect];
  self.modalMediator = nil;
}

@end
