// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/translate/translate_infobar_modal_overlay_coordinator.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/translate/translate_infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/translate/translate_infobar_modal_overlay_mediator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_language_selection_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_language_selection_table_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface TranslateInfobarModalOverlayCoordinator () <
    TranslateInfobarModalOverlayMediatorDelegate>
// Redefine ModalConfiguration properties as readwrite.
@property(nonatomic, readwrite) OverlayRequestMediator* modalMediator;
@property(nonatomic, readwrite) UIViewController* modalViewController;
// The request's config.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
@end

@implementation TranslateInfobarModalOverlayCoordinator

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

#pragma mark - TranslateInfobarModalOverlayMediatorDelegate

- (void)showChangeSourceLanguageOptions {
  InfobarTranslateLanguageSelectionTableViewController* languageSelectionTVC =
      [[InfobarTranslateLanguageSelectionTableViewController alloc]
                 initWithDelegate:[self translateModalOverlayMediator]
          selectingSourceLanguage:YES];
  languageSelectionTVC.title = l10n_util::GetNSString(
      IDS_IOS_TRANSLATE_INFOBAR_SELECT_LANGUAGE_MODAL_TITLE);
  TranslateInfobarModalOverlayMediator* translateModalMediator =
      [self translateModalOverlayMediator];
  translateModalMediator.sourceLanguageSelectionConsumer = languageSelectionTVC;
  [self.modalViewController.navigationController
      pushViewController:languageSelectionTVC
                animated:YES];
}

- (void)showChangeTargetLanguageOptions {
  InfobarTranslateLanguageSelectionTableViewController* languageSelectionTVC =
      [[InfobarTranslateLanguageSelectionTableViewController alloc]
                 initWithDelegate:[self translateModalOverlayMediator]
          selectingSourceLanguage:NO];
  languageSelectionTVC.title = l10n_util::GetNSString(
      IDS_IOS_TRANSLATE_INFOBAR_SELECT_LANGUAGE_MODAL_TITLE);
  TranslateInfobarModalOverlayMediator* translateModalMediator =
      [self translateModalOverlayMediator];
  translateModalMediator.targetLanguageSelectionConsumer = languageSelectionTVC;
  [self.modalViewController.navigationController
      pushViewController:languageSelectionTVC
                animated:YES];
}

#pragma mark - Private

- (TranslateInfobarModalOverlayMediator*)translateModalOverlayMediator {
  return base::apple::ObjCCastStrict<TranslateInfobarModalOverlayMediator>(
      self.modalMediator);
}

@end

@implementation TranslateInfobarModalOverlayCoordinator (ModalConfiguration)

- (void)configureModal {
  DCHECK(!self.modalMediator);
  DCHECK(!self.modalViewController);
  TranslateInfobarModalOverlayMediator* modalMediator =
      [[TranslateInfobarModalOverlayMediator alloc]
          initWithRequest:self.request];
  InfobarTranslateTableViewController* modalViewController =
      [[InfobarTranslateTableViewController alloc]
          initWithDelegate:modalMediator
               prefService:self.browser->GetProfile()->GetPrefs()];
  modalMediator.consumer = modalViewController;
  modalMediator.translateMediatorDelegate = self;
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
