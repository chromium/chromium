// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_CREDENTIAL_PROVIDER_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_CREDENTIAL_PROVIDER_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/common/credential_provider/credential_store.h"
#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"

// Exposes CredentialProviderViewController's private properties for tests only.
@interface CredentialProviderViewController (Testing)

// Interface for the persistent credential store.
@property(nonatomic, strong) id<CredentialStore> credentialStore;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_CREDENTIAL_PROVIDER_VIEW_CONTROLLER_TESTING_H_
