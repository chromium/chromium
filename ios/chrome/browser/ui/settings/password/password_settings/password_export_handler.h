// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_EXPORT_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_EXPORT_HANDLER_H_

// Protocol which handles showing alerts or ActivityViews in response to steps
// in the password export flow – essentially, the parts which are not allowed
// to happen in the ViewController.
@protocol PasswordExportHandler

// Displays the "share sheet" UI with the activity items provided by the
// password exporter.
- (void)showActivityViewWithActivityItems:(NSArray*)activityItems
                        completionHandler:
                            (void (^)(NSString*, BOOL, NSArray*, NSError*))
                                completionHandler;

// Displays an error message. The error message is already localized when
// provided to this method; the implementer should simply show it.
- (void)showExportErrorAlertWithLocalizedReason:(NSString*)errorReason;

// Shows an alert while the payload for export is being prepared. The purpose
// is similar to a loading spinner.
- (void)showPreparingPasswordsAlert;

// Shows a dialog requiring the user to set a device passcode before proceeding.
- (void)showSetPasscodeForPasswordExportDialog;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_EXPORT_HANDLER_H_
