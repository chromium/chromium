// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"

struct ManagementState;

// Consumer protocol for manage sync settings.
@protocol ManageSyncSettingsConsumer <NSObject>

// Returns the table view model.
@property(nonatomic, strong, readonly)
    TableViewModel<TableViewItem*>* tableViewModel;

// Inserts sections at `sections` indexes. Does nothing if the model is not
// loaded yet.
- (void)insertSections:(NSIndexSet*)sections rowAnimation:(BOOL)rowAnimation;

// Deletes sections at `sections` indexes. Does nothing if the model is not
// loaded yet.
- (void)deleteSections:(NSIndexSet*)section rowAnimation:(BOOL)rowAnimation;

// Reloads only a specific `item`. Does nothing if the model is not loaded
// yet.
- (void)reloadItem:(TableViewItem*)item;

// Reloads `sections`. Does nothing if the model is not loaded yet.
- (void)reloadSections:(NSIndexSet*)sections;

// Updates the primary account details.
- (void)updatePrimaryAccountWithAvatarImage:(UIImage*)avatarImage
                                       name:(NSString*)name
                                      email:(NSString*)email
                            managementState:(ManagementState)managementState;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_CONSUMER_H_
