// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/permissions/permissions_infobar_modal_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/modals/permissions/infobar_permissions_table_view_controller.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/permissions/permissions_infobar_modal_overlay_mediator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface PermissionsInfobarModalOverlayCoordinator ()

// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, strong, readwrite)
    PermissionsInfobarModalOverlayMediator* modalMediator;
@property(nonatomic, strong, readwrite) UIViewController* modalViewController;
// The request's config.
@property(nonatomic, assign, readonly)
    DefaultInfobarOverlayRequestConfig* config;

@end

@implementation PermissionsInfobarModalOverlayCoordinator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - Public

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

@end

@implementation PermissionsInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  DCHECK(!self.modalMediator);
  DCHECK(!self.modalViewController);
  PermissionsInfobarModalOverlayMediator* modalMediator =
      [[PermissionsInfobarModalOverlayMediator alloc]
          initWithRequest:self.request];
  InfobarPermissionsTableViewController* modalViewController =
      [[InfobarPermissionsTableViewController alloc]
          initWithDelegate:modalMediator];

  modalViewController.title =
      l10n_util::GetNSString(IDS_IOS_PERMISSIONS_INFOBAR_MODAL_TITLE);
  modalViewController.presentationHandler = self;

  modalMediator.consumer = modalViewController;
  self.modalMediator = modalMediator;
  self.modalViewController = modalViewController;
}

- (void)resetModal {
  DCHECK(self.modalMediator);
  DCHECK(self.modalViewController);

  [self.modalMediator disconnect];
  self.modalMediator = nil;
  self.modalViewController = nil;
}

@end
