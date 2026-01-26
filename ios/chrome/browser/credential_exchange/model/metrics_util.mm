// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/metrics_util.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "ios/chrome/browser/credential_exchange/model/import_stats.h"

namespace {

constexpr char kCredentialExchangeReceivedCountHistogram[] =
    "IOS.CredentialExchange.CredentialsReceivedCount";

// Logs the given `count` to a credential received count histogram with
// `suffix`.
void LogReceivedCount(std::string_view suffix, NSInteger count) {
  if (count <= 0) {
    return;
  }
  base::UmaHistogramCounts100(
      base::StrCat({kCredentialExchangeReceivedCountHistogram, suffix}),
      static_cast<int>(count));
}

}  // namespace

void LogImportStats(ImportStats* stats) {
  LogReceivedCount(".Address", stats.addressCount);
  LogReceivedCount(".APIKey", stats.apiKeyCount);
  LogReceivedCount(".BasicAuthentication", stats.basicAuthenticationCount);
  LogReceivedCount(".CreditCard", stats.creditCardCount);
  LogReceivedCount(".CustomFields", stats.customFieldsCount);
  LogReceivedCount(".DriversLicense", stats.driversLicenseCount);
  LogReceivedCount(".GeneratedPassword", stats.generatedPasswordCount);
  LogReceivedCount(".IdentityDocument", stats.identityDocumentCount);
  LogReceivedCount(".ItemReference", stats.itemReferenceCount);
  LogReceivedCount(".Note", stats.noteCount);
  LogReceivedCount(".NoteForPassword", stats.noteForPasswordCount);
  LogReceivedCount(".Passkey", stats.passkeyCount);
  LogReceivedCount(".Passport", stats.passportCount);
  LogReceivedCount(".PersonName", stats.personNameCount);
  LogReceivedCount(".SSHKey", stats.sshKeyCount);
  LogReceivedCount(".TOTP", stats.totpCount);
  LogReceivedCount(".WiFi", stats.wifiCount);
}
