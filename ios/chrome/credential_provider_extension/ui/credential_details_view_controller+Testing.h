// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_DETAILS_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_DETAILS_VIEW_CONTROLLER_TESTING_H_

// Testing category exposing a private method of CredentialDetailsViewController
// for tests.
@interface CredentialDetailsViewController (Testing)

// Formats and returns the passkey creation date to be displayed in the UI.
- (NSString*)formattedDateForPasskeyCreationDate:(NSDate*)creationDate;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_DETAILS_VIEW_CONTROLLER_TESTING_H_
