// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_UTILS_CREDENTIAL_PROVIDER_SETTINGS_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_UTILS_CREDENTIAL_PROVIDER_SETTINGS_UTILS_H_

// Enum which represents the different places in the app where the iOS prompt to
// set the app as a credential provider can be triggered.
enum class TurnOnCredentialProviderExtensionPromptSource {
  kPasswordSettings,
  kCredentialProviderExtensionPromo,
};

// Opens the iOS settings for the user to set the app as a credential provider.
// Directly opens the iOS credential provider settings when the API is
// available. Otherwise, falls back on opening the iOS settings homepage.
void OpenIOSCredentialProviderSettings();

// Records whether the user has decided to set the app as a credential provider
// when shown the iOS "Turn on AutoFill" prompt.
void RecordTurnOnCredentialProviderExtensionPromptOutcome(
    TurnOnCredentialProviderExtensionPromptSource source,
    bool app_was_enabled_for_autofill);

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_UTILS_CREDENTIAL_PROVIDER_SETTINGS_UTILS_H_
