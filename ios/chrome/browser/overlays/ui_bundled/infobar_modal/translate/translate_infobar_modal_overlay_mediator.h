// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_TRANSLATE_TRANSLATE_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_TRANSLATE_TRANSLATE_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_language_selection_consumer.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_language_selection_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_translate_modal_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/translate/translate_infobar_modal_overlay_mediator_delegate.h"

// Mediator that configures the modal UI for a Translate infobar.
@interface TranslateInfobarModalOverlayMediator
    : InfobarModalOverlayMediator <InfobarTranslateModalDelegate,
                                   InfobarTranslateLanguageSelectionDelegate>

// The consumer that is configured by this mediator.  Setting to a new value
// configures the new consumer.
@property(nonatomic, weak) id<InfobarTranslateModalConsumer> consumer;

// The consumer selecting the source language to be configured with this
// mediator.
@property(nonatomic, weak) id<InfobarTranslateLanguageSelectionConsumer>
    sourceLanguageSelectionConsumer;

// The consumer selecting the target language to be configured with this
// mediator.
@property(nonatomic, weak) id<InfobarTranslateLanguageSelectionConsumer>
    targetLanguageSelectionConsumer;

// Delegate to communicate user actions to change the UI presentation.
@property(nonatomic, weak) id<TranslateInfobarModalOverlayMediatorDelegate>
    translateMediatorDelegate;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_TRANSLATE_TRANSLATE_INFOBAR_MODAL_OVERLAY_MEDIATOR_H_
