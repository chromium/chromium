// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

// Delegate to handle Translate Infobar Modal actions.
@protocol InfobarTranslateModalDelegate <InfobarModalDelegate>

// Indicates the user chose to undo the translation (i.e. show the page in its
// original language).
- (void)showSourceLanguage;

// Indicates the user changed the source/target language and wishes to Translate
// again.
- (void)translateWithNewLanguages;

// Indicates the user chose to show options to change the source target
// language.
- (void)showChangeSourceLanguageOptions;

// Indicates the user chose to show options to change the source target
// language.
- (void)showChangeTargetLanguageOptions;

// Indicates the user chose to always translate sites in the source language.
// Triggers a translate as well.
- (void)alwaysTranslateSourceLanguage;
// Indicates the user chose to undo always translate sites in the source
// language.
- (void)undoAlwaysTranslateSourceLanguage;

// Indicates the user chose to never translate sites in the source language.
- (void)neverTranslateSourceLanguage;
// Indicates the user chose to undo never translate sites in the source
// language.
- (void)undoNeverTranslateSourceLanguage;

// Indicates the user chose to never translate for this site.
- (void)neverTranslateSite;
// Indicates the user chose to undo never translate for this site.
- (void)undoNeverTranslateSite;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_MODAL_DELEGATE_H_
