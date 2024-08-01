// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/passwords/password_infobar_modal_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_password_table_view_controller.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/passwords/password_infobar_modal_overlay_mediator.h"

@interface PasswordInfobarModalOverlayCoordinator ()
// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, readwrite) OverlayRequestMediator* modalMediator;
@property(nonatomic, readwrite) UIViewController* modalViewController;
// The request's config.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
@end

@implementation PasswordInfobarModalOverlayCoordinator

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

@implementation PasswordInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  DCHECK(!self.modalMediator);
  DCHECK(!self.modalViewController);
  PasswordInfobarModalOverlayMediator* modalMediator =
      [[PasswordInfobarModalOverlayMediator alloc]
          initWithRequest:self.request];
  InfobarPasswordTableViewController* modalViewController =
      [[InfobarPasswordTableViewController alloc]
          initWithDelegate:modalMediator
                      type:self.config->infobar_type()];
  modalMediator.consumer = modalViewController;
  self.modalMediator = modalMediator;
  self.modalViewController = modalViewController;
}

- (void)resetModal {
  DCHECK(self.modalMediator);
  DCHECK(self.modalViewController);
  self.modalMediator = nil;
  self.modalViewController = nil;
}

@end
