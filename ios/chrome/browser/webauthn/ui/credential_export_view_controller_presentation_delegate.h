// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

// Delegate for CredentialExportViewController.
@protocol CredentialExportViewControllerPresentationDelegate <NSObject>

// Called when the user accepts the export flow.
- (void)userDidStartExport;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
