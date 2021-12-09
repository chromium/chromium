// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_FEATURE_FLAGS_H_

#import <Foundation/Foundation.h>

// Whether the entire password creation feature is enabled.
BOOL IsPasswordCreationEnabled();

// Whether password creation is enabled for this user by preference.
BOOL IsPasswordCreationUserRestricted();

BOOL IsCredentialProviderExtensionPromoEnabled();

// Whether the password manager branding UI update feature is enabled.
BOOL IsPasswordManagerBrandingUpdateEnable();

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_FEATURE_FLAGS_H_
