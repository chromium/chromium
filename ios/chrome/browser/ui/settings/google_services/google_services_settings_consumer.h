// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"

// Consumer protocol for Google services settings.
@protocol GoogleServicesSettingsConsumer <NSObject>

// Returns the table view model.
@property(nonatomic, strong, readonly)
    TableViewModel<TableViewItem*>* tableViewModel;

// Inserts sections at `sections` indexes. Does nothing if the model is not
// loaded yet.
- (void)insertSections:(NSIndexSet*)sections;

// Deletes sections at `sections` indexes. Does nothing if the model is not
// loaded yet.
- (void)deleteSections:(NSIndexSet*)sections;

// Reloads `sections`. Does nothing if the model is not loaded yet.
- (void)reloadSections:(NSIndexSet*)sections;

// Reloads only a specific `item`. Does nothing if the model is not loaded
// yet.
- (void)reloadItem:(TableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_CONSUMER_H_
