// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol InfobarTranslateModalDelegate;

// InfobarTranslateTableViewController represents the content for the Translate
// InfobarModal.
@interface InfobarTranslateTableViewController : ChromeTableViewController

- (instancetype)initWithDelegate:
    (id<InfobarTranslateModalDelegate>)modalDelegate NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// The source language to be displayed in the InfobarModal.
@property(nonatomic, copy) NSString* sourceLanguage;

// The target language to be displayed in the InfobarModal.
@property(nonatomic, copy) NSString* targetLanguage;

// The text for the Infobar action button (i.e. translate or show
// original)
@property(nonatomic, copy) NSString* translateButtonText;

// Whether the current preference is set to always translate |targetLanguage|.
@property(nonatomic, assign) BOOL shouldAlwaysTranslateSourceLanguage;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_TABLE_VIEW_CONTROLLER_H_
