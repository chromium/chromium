// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_REKOR_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_REKOR_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "device/fido/enclave/verify/hash.h"

namespace device::enclave {

// Struct representing a verification object in a Rekor LogEntry. The
// verification object in Rekor also contains an inclusion proof. Since we
// currently don't verify the inclusion proof in the client, it is omitted from
// this struct.
struct LogEntryVerification {
  // Base64-encoded signature over the body, integrated_time, log_id, and
  // log_index of the Rekor LogEntry.
  std::string_view signed_entry_timestamp;
};

// Struct representing a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/log_entry.go#L89.>
struct LogEntry {
  std::string_view body;
  base::Time integrated_time;
  // This is the SHA256 hash of the DER-encoded public key for the log at the
  // time the entry was included in the log
  // Pattern: ^[0-9a-fA-F]{64}$
  std::string_view log_id;
  uint64_t log_index;
  // Includes a signature over the body, integrated_time, log_id, and log_index.
  std::optional<LogEntryVerification> verification;
};

// Struct representing a public key included in the body of a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/rekord_v001_schema.go#L551.>
struct PublicKey {
  // Base64 content of a public key.
  std::string_view content;
};

// Struct representing a signature in the body of a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/rekord_v001_schema.go#L383>
struct GenericSignature {
  // Base64 content that is signed.
  std::string_view content;
  std::string_view format;
  PublicKey public_key;
};

// Struct representing the hashed data in the body of a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/rekord_v001_schema.go#L179.>
struct Data {
  Hash hash;
};

// Struct representing the `spec` in the body of a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/rekord_v001_schema.go#L39.>
struct Spec {
  Data data;
  GenericSignature generic_signature;
};

// Struct representing the body in a Rekor LogEntry.
struct Body {
  std::string_view api_version;
  std::string_view kind;
  Spec spec;
};

// Convenient struct for verifying the `signed_entry_timestamp` in a Rekor
// LogEntry.
//
// This bundle can be verified using the public key from Rekor. The public key
// can be obtained from the `/api/v1/log/publicKey` Rest API. For
// `sigstore.dev`, it is a PEM-encoded x509/PKIX public key.
struct RekorSignatureBundle {
  RekorSignatureBundle(std::vector<uint8_t> canonicalized,
                       std::vector<uint8_t> signature);
  RekorSignatureBundle();
  ~RekorSignatureBundle();

  // Canonicalized JSON representation, based on RFC 8785 rules, of a subset
  // of a Rekor LogEntry fields that are signed to generate
  // `signed_entry_timestamp` (also a field in the Rekor LogEntry). These
  // fields include body, integratedTime, logID and logIndex.
  std::vector<uint8_t> canonicalized;
  // The signature over the canonicalized JSON document.
  std::vector<uint8_t> signature;
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_REKOR_H_
