// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_LANGUAGE_SELECTION_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_LANGUAGE_SELECTION_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_language_selection_consumer.h"

@protocol InfobarTranslateLanguageSelectionDelegate;

// InfobarTranslateLanguageSelectionTableViewController represents the content
// for selecting a different source or target language depending on
// `sourceLanguage` passed in the init method.
@interface InfobarTranslateLanguageSelectionTableViewController
    : LegacyChromeTableViewController <
          InfobarTranslateLanguageSelectionConsumer>

- (instancetype)initWithDelegate:(id<InfobarTranslateLanguageSelectionDelegate>)
                                     langageSelectionDelegate
         selectingSourceLanguage:(BOOL)sourceLanguage NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_LANGUAGE_SELECTION_TABLE_VIEW_CONTROLLER_H_
