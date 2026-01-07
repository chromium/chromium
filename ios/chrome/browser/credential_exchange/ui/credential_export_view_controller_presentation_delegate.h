// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"

@class CredentialGroupIdentifier;

// Delegate for CredentialExportViewController.
@protocol CredentialExportViewControllerPresentationDelegate <NSObject>

// Called when the user accepts the export flow.
- (void)userDidStartExport:(NSArray<CredentialGroupIdentifier*>*)selectedItems;

// Called when the user taps "Export to CSV".
- (void)exportCredentialsToCSV:
    (std::vector<password_manager::CredentialUIEntry>)credentials;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
