// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_TOKEN_BINDING_RESPONSE_ENCRYPTION_ERROR_H_
#define GOOGLE_APIS_GAIA_TOKEN_BINDING_RESPONSE_ENCRYPTION_ERROR_H_

// Encryption-related errors that might occur while processing a response to a
// request authorizing with bound tokens.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(TokenBindingResponseEncryptionError)
enum class TokenBindingResponseEncryptionError {
  kResponseUnexpectedlyEncrypted = 0,
  kDecryptionFailed = 1,
  kSuccessfullyDecrypted = 2,
  kSuccessNoEncryption = 3,
  kMaxValue = kSuccessNoEncryption
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:TokenBindingResponseEncryptionError)

#endif  // GOOGLE_APIS_GAIA_TOKEN_BINDING_RESPONSE_ENCRYPTION_ERROR_H_
