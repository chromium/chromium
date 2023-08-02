// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_coordinator.h"

#import "base/check.h"
#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#import "ios/chrome/browser/overlays/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_table_view_controller.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface SaveCardInfobarModalOverlayCoordinator ()
// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, strong, readwrite) OverlayRequestMediator* modalMediator;
@property(nonatomic, strong, readwrite) UIViewController* modalViewController;
// The request's config.
@property(nonatomic, assign, readonly)
    DefaultInfobarOverlayRequestConfig* config;
// URL to load when the modal UI finishes dismissing.
@property(nonatomic, assign) GURL pendingURLToLoad;
@end

@implementation SaveCardInfobarModalOverlayCoordinator

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

#pragma mark - SaveCardInfobarModalOverlayMediatorDelegate

- (void)pendingURLToLoad:(GURL)URL {
  const GURL copied_URL(URL);
  self.pendingURLToLoad = GURL(copied_URL);
}

@end

@implementation SaveCardInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  DCHECK(!self.modalMediator);
  DCHECK(!self.modalViewController);
  SaveCardInfobarModalOverlayMediator* modalMediator =
      [[SaveCardInfobarModalOverlayMediator alloc]
          initWithRequest:self.request];
  InfobarSaveCardTableViewController* modalViewController =
      [[InfobarSaveCardTableViewController alloc]
          initWithModalDelegate:modalMediator];
  modalViewController.title =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
  modalMediator.consumer = modalViewController;
  modalMediator.save_card_delegate = self;
  self.modalMediator = modalMediator;
  self.modalViewController = modalViewController;
}

- (void)resetModal {
  DCHECK(self.modalMediator);
  DCHECK(self.modalViewController);
  if (!self.pendingURLToLoad.is_empty() && self.request) {
    autofill::AutofillSaveCardInfoBarDelegateMobile* delegate =
        static_cast<autofill::AutofillSaveCardInfoBarDelegateMobile*>(
            self.config->delegate());
    if (delegate) {
      delegate->OnLegalMessageLinkClicked(self.pendingURLToLoad);
    }
  }
  self.modalMediator = nil;
  self.modalViewController = nil;
}

@end
