// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/passwords/password_infobar_modal_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_password_table_view_controller.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/passwords/password_infobar_modal_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_modal::PasswordAction;

namespace {
// Returns the InfobarType that should be used to construct the
// InfobarPasswordTableViewController for `action`.
InfobarType GetTableViewInfobarType(PasswordAction action) {
  switch (action) {
    case PasswordAction::kSave:
      return InfobarType::kInfobarTypePasswordSave;
    case PasswordAction::kUpdate:
      return InfobarType::kInfobarTypePasswordUpdate;
  }
}
}  // namespace

@interface PasswordInfobarModalOverlayCoordinator ()
// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, readwrite) OverlayRequestMediator* modalMediator;
@property(nonatomic, readwrite) UIViewController* modalViewController;
// The request's config.
@property(nonatomic, readonly) PasswordInfobarModalOverlayRequestConfig* config;
@end

@implementation PasswordInfobarModalOverlayCoordinator

#pragma mark - Accessors

- (PasswordInfobarModalOverlayRequestConfig*)config {
  return self.request
             ? self.request
                   ->GetConfig<PasswordInfobarModalOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - Public

+ (const OverlayRequestSupport*)requestSupport {
  return PasswordInfobarModalOverlayRequestConfig::RequestSupport();
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
                      type:GetTableViewInfobarType(self.config->action())];
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
