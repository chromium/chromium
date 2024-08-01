// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_TRANSLATE_TRANSLATE_INFOBAR_MODAL_OVERLAY_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_TRANSLATE_TRANSLATE_INFOBAR_MODAL_OVERLAY_MEDIATOR_DELEGATE_H_

// Delegate to communicate modal user actions to show options to change
// languages.
@protocol TranslateInfobarModalOverlayMediatorDelegate

// Indicates the user chose to show options to change the source target
// language.
- (void)showChangeSourceLanguageOptions;
// Indicates the user chose to show options to change the source target
// language.
- (void)showChangeTargetLanguageOptions;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_MODAL_TRANSLATE_TRANSLATE_INFOBAR_MODAL_OVERLAY_MEDIATOR_DELEGATE_H_
