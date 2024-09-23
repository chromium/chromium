// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_checkup_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"

using password_manager::WarningType;

namespace {

const char kUserActionWithContextHistogram[] =
    "PasswordManager.BulkCheck.UserAction.IOS";

const char kGeneralUserActionHistogram[] =
    "PasswordManager.BulkCheck.UserAction.IOS.General";

// Enum representing the different types of interactions that a user can have
// with Password Check on iOS for a specific type of insecure credential.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. It must be kept in sync with
// PasswordCheckInteractionIOS in enums.xml.
enum class PasswordCheckInteractionIOS {
  kChangePasswordOnWebsite = 0,
  kEditPassword = 1,
  kRemovePassword = 2,
  kShowPassword = 3,
  kMuteWarning = 4,
  kUnmuteWarning = 5,
  kShowIssuesList = 6,
  kMaxValue = kShowIssuesList,
};

// Enum representing the different types of interactions that a user can have
// with Password Check on iOS not specific to a type of insecure credential.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. It must be kept in sync with
// PasswordCheckInteractionIOSWithoutContext in enums.xml.
enum class PasswordCheckInteractionIOSWithoutContext {
  kAutomaticPasswordCheck = 0,
  kManualPasswordCheck = 1,
  kOpenCheckupHomepage = 2,
  kMaxValue = kOpenCheckupHomepage,
};

// Gets the histogram name for the given context.
std::string GetHistogramForContext(WarningType context) {
  switch (context) {
    case WarningType::kCompromisedPasswordsWarning:
      return base::StrCat({kUserActionWithContextHistogram, ".Compromised"});
    case WarningType::kReusedPasswordsWarning:
      return base::StrCat({kUserActionWithContextHistogram, ".Reused"});
    case WarningType::kWeakPasswordsWarning:
      return base::StrCat({kUserActionWithContextHistogram, ".Weak"});
    case WarningType::kDismissedWarningsWarning:
      return base::StrCat(
          {kUserActionWithContextHistogram, ".MutedCompromised"});
    case WarningType::kNoInsecurePasswordsWarning:
      NOTREACHED();
  }
}

void LogPasswordCheckInteraction(PasswordCheckInteractionIOS interaction,
                                 WarningType context) {
  base::UmaHistogramEnumeration(kUserActionWithContextHistogram, interaction);
  base::UmaHistogramEnumeration(GetHistogramForContext(context), interaction);
}

void LogGeneralPasswordCheckInteraction(
    PasswordCheckInteractionIOSWithoutContext interaction) {
  base::UmaHistogramEnumeration(kGeneralUserActionHistogram, interaction);
}

}  // namespace

namespace password_manager {

const char kInsecureCredentialsCountHistogram[] =
    "PasswordManager.BulkCheck.InsecureCredentials.Count";

const char kUnmutedInsecureCredentialsCountHistogram[] =
    "PasswordManager.BulkCheck.InsecureCredentials.Unmuted.Count";

void LogChangePasswordOnWebsite(WarningType context) {
  LogPasswordCheckInteraction(
      PasswordCheckInteractionIOS::kChangePasswordOnWebsite, context);
}

void LogEditPassword(WarningType context) {
  LogPasswordCheckInteraction(PasswordCheckInteractionIOS::kEditPassword,
                              context);
}

void LogDeletePassword(WarningType context) {
  LogPasswordCheckInteraction(PasswordCheckInteractionIOS::kRemovePassword,
                              context);
}

void LogRevealPassword(WarningType context) {
  LogPasswordCheckInteraction(PasswordCheckInteractionIOS::kShowPassword,
                              context);
}

void LogOpenPasswordIssuesList(WarningType context) {
  LogPasswordCheckInteraction(PasswordCheckInteractionIOS::kShowIssuesList,
                              context);
}

void LogMuteCompromisedWarning() {
  LogPasswordCheckInteraction(PasswordCheckInteractionIOS::kMuteWarning,
                              WarningType::kCompromisedPasswordsWarning);
}

void LogUnmuteCompromisedWarning() {
  LogPasswordCheckInteraction(PasswordCheckInteractionIOS::kUnmuteWarning,
                              WarningType::kDismissedWarningsWarning);
}

void LogStartPasswordCheckManually() {
  LogGeneralPasswordCheckInteraction(
      PasswordCheckInteractionIOSWithoutContext::kManualPasswordCheck);
}

void LogStartPasswordCheckAutomatically() {
  LogGeneralPasswordCheckInteraction(
      PasswordCheckInteractionIOSWithoutContext::kAutomaticPasswordCheck);
}

void LogOpenPasswordCheckupHomePage() {
  LogGeneralPasswordCheckInteraction(
      PasswordCheckInteractionIOSWithoutContext::kOpenCheckupHomepage);
}

void LogCountOfInsecureUsernamePasswordPairs(int count) {
  base::UmaHistogramCounts1000(kInsecureCredentialsCountHistogram, count);
}

void LogCountOfUnmutedInsecureUsernamePasswordPairs(int count) {
  base::UmaHistogramCounts1000(kUnmutedInsecureCredentialsCountHistogram,
                               count);
}

}  // namespace password_manager
