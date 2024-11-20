// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_FEATURE_FLAGS_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_FEATURE_FLAGS_H_

#import <Foundation/Foundation.h>

// Whether password creation is enabled for this user by preference.
BOOL IsPasswordCreationUserEnabled();

// Whether password creation enabled/disabled state is controlled by an
// enterprise policy.
BOOL IsPasswordCreationManaged();

// Whether password sync is enabled for this user.
BOOL IsPasswordSyncEnabled();
#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_FEATURE_FLAGS_H_
