// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"

// Section identifiers for the Suggestions from Gemini table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSuggestionsFromGemini = kSectionIdentifierEnumZero,
  SectionIdentifierHelpImprove,
};

// Item types for the Suggestions from Gemini table view.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFindAndFillSwitch = kItemTypeEnumZero,
  ItemTypeManageConnectedApps,
  ItemTypeHelpImprove,
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_CONSTANTS_H_
