// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_UTILS_CREDENTIAL_PROVIDER_SETTINGS_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_UTILS_CREDENTIAL_PROVIDER_SETTINGS_UTILS_H_

// Opens the iOS settings for the user to set the app as a credential provider.
// Directly opens the iOS credential provider settings when the API is
// available. Otherwise, falls back on opening the iOS settings homepage.
void OpenIOSCredentialProviderSettings();

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_UTILS_CREDENTIAL_PROVIDER_SETTINGS_UTILS_H_
