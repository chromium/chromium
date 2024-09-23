// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/parcel_tracking/parcel_tracking_infobar_modal_overlay_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_infobar_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_table_view_controller.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/parcel_tracking/parcel_tracking_infobar_modal_overlay_mediator.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

@interface ParcelTrackingInfobarModalOverlayCoordinator ()
// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, readwrite)
    ParcelTrackingInfobarModalOverlayMediator* modalMediator;
@property(nonatomic, readwrite) UIViewController* modalViewController;
// The request's config.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
@end

@implementation ParcelTrackingInfobarModalOverlayCoordinator

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

#pragma mark - InfobarParcelTrackingPresenter

- (void)showReportIssueView {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler
      showReportAnIssueFromViewController:self.baseViewController
                                   sender:UserFeedbackSender::ParcelTracking];
}

@end

@implementation
ParcelTrackingInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  DCHECK(!self.modalMediator);
  DCHECK(!self.modalViewController);
  ParcelTrackingInfobarModalOverlayMediator* modalMediator =
      [[ParcelTrackingInfobarModalOverlayMediator alloc]
          initWithRequest:self.request];
  self.modalMediator = modalMediator;
  InfobarParcelTrackingTableViewController* modalViewController =
      [[InfobarParcelTrackingTableViewController alloc]
          initWithDelegate:self.modalMediator
                 presenter:self];
  modalMediator.consumer = modalViewController;
  self.modalViewController = modalViewController;
}

- (void)resetModal {
  DCHECK(self.modalMediator);
  DCHECK(self.modalViewController);
  self.modalMediator = nil;
  self.modalViewController = nil;
}

@end
