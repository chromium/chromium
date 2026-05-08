// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class TravelInfoTableViewController;

// Protocol for actions triggered by the Travel Info view.
@protocol TravelInfoMutator <NSObject>

// Called when the user taps on a travel info item.
- (void)didSelectTravelInfoItem:(TableViewItem*)item;

@end

// Delegate for presentation events related to TravelInfoTableViewController.
@protocol TravelInfoTableViewControllerDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)travelInfoTableViewControllerDidRemove:
    (TravelInfoTableViewController*)controller;

@end

// The TableView for Travel Info settings page.
@interface TravelInfoTableViewController
    : SettingsRootTableViewController <TravelInfoConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak) id<TravelInfoTableViewControllerDelegate> delegate;

// Mutator for actions in the view.
@property(nonatomic, weak) id<TravelInfoMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_TABLE_VIEW_CONTROLLER_H_
