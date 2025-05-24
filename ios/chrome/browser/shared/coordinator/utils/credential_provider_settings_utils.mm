// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/utils/credential_provider_settings_utils.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/password_auto_fill/password_auto_fill_api.h"

namespace {

// Name of the histogram that logs the outcome of the prompt that allows the
// user to set the app as a credential provider.
constexpr char kTurnOnPromptOutcomeHistogramPrefix[] =
    "IOS.CredentialProviderExtension.TurnOnPromptOutcome.";

// Returns the string representation of `source`.
// LINT.IfChange(TurnOnCredentialProviderExtensionPromptSourceToString)
std::string TurnOnCredentialProviderExtensionPromptSourceToString(
    TurnOnCredentialProviderExtensionPromptSource source) {
  switch (source) {
    case TurnOnCredentialProviderExtensionPromptSource::kPasswordSettings:
      return "PasswordSettings";
    case TurnOnCredentialProviderExtensionPromptSource::
        kCredentialProviderExtensionPromo:
      return "Promo";
  }
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/histograms.xml:IOSTurnOnCredentialProviderExtensionPromptSource)

}  // namespace

void OpenIOSCredentialProviderSettings() {
  if (!IOSPasskeysM2Enabled()) {
    ios::provider::PasswordsInOtherAppsOpensSettings();
    return;
  }

  // If available, use the API that allows to directly open the iOS credential
  // provider settings.
  if (@available(iOS 17.0, *)) {
    [ASSettingsHelper openCredentialProviderAppSettingsWithCompletionHandler:^(
                          NSError* error) {
      if (error) {
        ios::provider::PasswordsInOtherAppsOpensSettings();
      }
    }];
  } else {
    ios::provider::PasswordsInOtherAppsOpensSettings();
  }
}

void RecordTurnOnCredentialProviderExtensionPromptOutcome(
    TurnOnCredentialProviderExtensionPromptSource source,
    bool app_was_enabled_for_autofill) {
  base::UmaHistogramBoolean(
      kTurnOnPromptOutcomeHistogramPrefix +
          TurnOnCredentialProviderExtensionPromptSourceToString(source),
      app_was_enabled_for_autofill);
}
