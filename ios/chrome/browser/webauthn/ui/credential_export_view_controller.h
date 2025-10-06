// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol CredentialExportViewControllerPresentationDelegate;

@interface CredentialExportViewController : UITableViewController

// Delegate for handling dismissal of the view.
@property(nonatomic, weak)
    id<CredentialExportViewControllerPresentationDelegate>
        delegate;

- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_H_
