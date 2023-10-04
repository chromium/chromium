// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_PRIVATE_H_

#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"

@interface SafetyCheckMediator ()

// SettingsCheckItem used to display the state of the update check.
@property(nonatomic, strong) SettingsCheckItem* updateCheckItem;

// Current state of the update check.
@property(nonatomic, assign) UpdateCheckRowStates updateCheckRowState;

// Previous on load or finished check state of the update check.
@property(nonatomic, assign) UpdateCheckRowStates previousUpdateCheckRowState;

// SettingsCheckItem used to display the state of the password check.
@property(nonatomic, strong) SettingsCheckItem* passwordCheckItem;

// Current state of the password check.
@property(nonatomic, assign) PasswordCheckRowStates passwordCheckRowState;

// Previous on load or finished check state of the password check.
@property(nonatomic, assign)
    PasswordCheckRowStates previousPasswordCheckRowState;

// SettingsCheckItem used to display the state of the Safe Browsing check.
@property(nonatomic, strong) SettingsCheckItem* safeBrowsingCheckItem;

// Current state of the Safe Browsing check.
@property(nonatomic, assign)
    SafeBrowsingCheckRowStates safeBrowsingCheckRowState;

// Previous on load or finished check state of the Safe Browsing check.
@property(nonatomic, assign)
    SafeBrowsingCheckRowStates previousSafeBrowsingCheckRowState;

// Row button to start the safety check.
@property(nonatomic, strong) TableViewTextItem* checkStartItem;

// Current state of the start safety check row button.
@property(nonatomic, assign) CheckStartStates checkStartState;

// Whether or not a safety check just ran.
@property(nonatomic, assign) BOOL checkDidRun;

// Current state of password check.
@property(nonatomic, assign) PasswordCheckState currentPasswordCheckState;

// Preference value for Safe Browsing.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingPreference;

// Preference value for Enhanced Safe Browsing.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* enhancedSafeBrowsingPreference;

- (void)checkAndReconfigureSafeBrowsingState;
- (void)resetsCheckStartItemIfNeeded;
- (void)passwordCheckStateDidChange:(PasswordCheckState)state;
- (void)reconfigurePasswordCheckItem;
- (void)reconfigureUpdateCheckItem;
- (void)reconfigureSafeBrowsingCheckItem;
- (void)reconfigureCheckStartSection;
- (void)handleOmahaResponse:(const UpgradeRecommendedDetails&)details;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_PRIVATE_H_
