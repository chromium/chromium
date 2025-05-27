// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_ADDRESS_BAR_PREFERENCE_ADDRESS_BAR_PREFERENCE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_ADDRESS_BAR_PREFERENCE_ADDRESS_BAR_PREFERENCE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/address_bar_preference/address_bar_preference_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@protocol AddressBarPreferenceServiceDelegate;
@class AddressBarPreferenceViewController;

// Delegate for the presentation events related to
// `AddressBarPreferenceViewController`.
@protocol AddressBarPreferenceViewControllerPresentationDelegate

// Called when the view controller is removed from its parent.
- (void)addressBarPreferenceViewControllerWasRemoved:
    (AddressBarPreferenceViewController*)controller;

@end

// This class is the view controller for the address bar preference setting.
@interface AddressBarPreferenceViewController
    : SettingsRootTableViewController <AddressBarPreferenceConsumer,
                                       SettingsControllerProtocol>

@property(nonatomic, weak) id<AddressBarPreferenceServiceDelegate>
    prefServiceDelegate;

@property(nonatomic, weak)
    id<AddressBarPreferenceViewControllerPresentationDelegate>
        presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_ADDRESS_BAR_PREFERENCE_ADDRESS_BAR_PREFERENCE_VIEW_CONTROLLER_H_
