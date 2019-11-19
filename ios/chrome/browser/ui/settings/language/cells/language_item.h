// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_CELLS_LANGUAGE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_CELLS_LANGUAGE_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"

#include <string>

// Contains the model data for a language in the Language Settings page.
@interface LanguageItem : TableViewMultiDetailTextItem

// The language code for this language.
@property(nonatomic, assign) std::string languageCode;

// Whether the language is the Translate target language.
@property(nonatomic, assign, getter=isTargetLanguage) BOOL targetLanguage;

// Whether the language is Translate-blocked (Translate is not offered).
@property(nonatomic, assign, getter=isBlocked) BOOL blocked;

// Whether the language is supported by the Translate server.
@property(nonatomic, assign) BOOL supportsTranslate;

// Whether Translate can be offered for the language (it can be unblocked). True
// if the language is supported by the Translate server, it is not the last
// Translate-blocked language, and it is not the Translate target language.
@property(nonatomic, assign) BOOL canOfferTranslate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_CELLS_LANGUAGE_ITEM_H_
