// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_CREDENTIAL_PROVIDER_VIEW_CONTROLLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_CREDENTIAL_PROVIDER_VIEW_CONTROLLER_H_

#import <AuthenticationServices/AuthenticationServices.h>

// This is the main entry point for the extension. The system interacts with it
// via the methods defined in ASCredentialProviderViewController.
@interface CredentialProviderViewController : ASCredentialProviderViewController
@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_CREDENTIAL_PROVIDER_VIEW_CONTROLLER_H_
