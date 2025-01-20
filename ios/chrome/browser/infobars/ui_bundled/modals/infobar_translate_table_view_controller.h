// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_TRANSLATE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_TRANSLATE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/infobars/ui_bundled/coordinators/infobar_translate_modal_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@protocol InfobarModalPresentationHandler;
@protocol InfobarTranslateModalDelegate;
class PrefService;

// InfobarTranslateTableViewController represents the content for the Translate
// InfobarModal.
@interface InfobarTranslateTableViewController
    : LegacyChromeTableViewController <InfobarTranslateModalConsumer>

- (instancetype)initWithDelegate:
                    (id<InfobarTranslateModalDelegate>)modalDelegate
                     prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Handler used to resize the modal.
@property(nonatomic, weak) id<InfobarModalPresentationHandler>
    presentationHandler;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_MODALS_INFOBAR_TRANSLATE_TABLE_VIEW_CONTROLLER_H_
