// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_COUNTRY_SELECTION_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_COUNTRY_SELECTION_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@class AutofillCountrySelectionTableViewController;
@class CountryItem;

// Protocol used by AutofillCountrySelectionTableViewController to communicate
// to its delegate.
@protocol AutofillCountrySelectionTableViewControllerDelegate

// Informs the delegate that user selected a country with the given country
// code.
- (void)didSelectCountry:(CountryItem*)selectedCountry;

@end

// Controller for the UI that allows the user to select a country.
@interface AutofillCountrySelectionTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// The designated initializer. `delegate` must not be nil and
// will not be retained.
- (instancetype)initWithDelegate:
                    (id<AutofillCountrySelectionTableViewControllerDelegate>)
                        delegate
                 selectedCountry:(NSString*)country
                    allCountries:(NSArray<CountryItem*>*)allCountries
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_COUNTRY_SELECTION_TABLE_VIEW_CONTROLLER_H_
