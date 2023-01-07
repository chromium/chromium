// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_LANGUAGE_SELECTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_LANGUAGE_SELECTION_DELEGATE_H_

// Delegate to handle Translate Infobar Language Selection changes.
@protocol InfobarTranslateLanguageSelectionDelegate

// Indicates the user chose to change the source  language to one named
// `language` at `languageIndex`.
- (void)didSelectSourceLanguageIndex:(int)languageIndex
                            withName:(NSString*)languageName;

// Indicates the user chose to change the source language to one named
// `language` at `languageIndex`.
- (void)didSelectTargetLanguageIndex:(int)languageIndex
                            withName:(NSString*)languageName;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_LANGUAGE_SELECTION_DELEGATE_H_
