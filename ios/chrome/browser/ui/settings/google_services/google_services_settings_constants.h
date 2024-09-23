// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the Google services settings table view.
extern NSString* const kGoogleServicesSettingsViewIdentifier;

// Accessibility identifier for Manage Sync cell.
extern NSString* const kManageSyncCellAccessibilityIdentifier;

// Accessibility identifier for the account list cell.
extern NSString* const kAccountListItemAccessibilityIdentifier;

// Accessibility identifier for the password leak check cell.
extern NSString* const kPasswordLeakCheckItemAccessibilityIdentifier;

// Accessibility identifier for the Safe Browsing cell.
extern NSString* const kSafeBrowsingItemAccessibilityIdentifier;

// Accessibility identifier for the encryption passphrase UITextField.
extern NSString* const
    kSyncEncryptionPassphraseTextFieldAccessibilityIdentifier;

// Accessibility identifier for the encryption passphrase table view.
extern NSString* const
    kSyncEncryptionPassphraseTableViewAccessibilityIdentifier;

// Accessibility identifier for the Allow Chrome Sign-in cell.
extern NSString* const kAllowSigninItemAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_CONSTANTS_H_
