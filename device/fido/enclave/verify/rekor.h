// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_REKOR_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_REKOR_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/json/json_value_converter.h"
#include "base/time/time.h"

namespace device::enclave {

// Struct representing a verification object in a Rekor LogEntry. The
// verification object in Rekor also contains an inclusion proof. Since we
// currently don't verify the inclusion proof in the client, it is omitted from
// this struct.
struct LogEntryVerification {
  static void RegisterJSONConverter(
      base::JSONValueConverter<LogEntryVerification>* converter);

  // Base64-encoded signature over the body, integrated_time, log_id, and
  // log_index of the Rekor LogEntry.
  std::string signed_entry_timestamp;
};

// Struct representing a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/log_entry.go#L89.>
struct COMPONENT_EXPORT(DEVICE_FIDO) LogEntry {
  LogEntry(std::string body,
           base::Time integrated_time,
           std::string log_id,
           uint64_t log_index,
           std::optional<LogEntryVerification> verification);
  LogEntry(const LogEntry& log_entry);
  LogEntry(LogEntry&& log_entry);
  LogEntry();
  ~LogEntry();

  static void RegisterJSONConverter(
      base::JSONValueConverter<LogEntry>* converter);

  std::string body;
  base::Time integrated_time;
  // This is the SHA256 hash of the DER-encoded public key for the log at the
  // time the entry was included in the log
  // Pattern: ^[0-9a-fA-F]{64}$
  std::string log_id;
  uint64_t log_index;
  // Includes a signature over the body, integrated_time, log_id, and log_index.
  std::optional<LogEntryVerification> verification;
};

// Struct representing a public key included in the body of a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/rekord_v001_schema.go#L551.>
struct PublicKey {
  static void RegisterJSONConverter(
      base::JSONValueConverter<PublicKey>* converter);

  // Base64 content of a public key.
  std::string content;
};

// Struct representing a signature in the body of a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/rekord_v001_schema.go#L383>
struct GenericSignature {
  static void RegisterJSONConverter(
      base::JSONValueConverter<GenericSignature>* converter);

  // Base64 content that is signed.
  std::string content;
  std::string format;
  PublicKey public_key;
};

enum HashType {
  kSHA256,
};

struct COMPONENT_EXPORT(DEVICE_FIDO) Hash {
  Hash(std::vector<uint8_t> bytes, HashType hash_type);
  Hash();
  ~Hash();
  Hash(const Hash& hash);

  static void RegisterJSONConverter(base::JSONValueConverter<Hash>* converter);

  std::vector<uint8_t> bytes;
  HashType hash_type = kSHA256;
};

// Struct representing the hashed data in the body of a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/rekord_v001_schema.go#L179.>
struct Data {
  static void RegisterJSONConverter(base::JSONValueConverter<Data>* converter);

  Hash hash;
};

// Struct representing the `spec` in the body of a Rekor LogEntry.
// Based on
// <https://github.com/sigstore/rekor/blob/2978cdc26fdf8f5bfede8459afd9735f0f231a2a/pkg/generated/models/rekord_v001_schema.go#L39.>
struct Spec {
  static void RegisterJSONConverter(base::JSONValueConverter<Spec>* converter);

  Data data;
  GenericSignature generic_signature;
};

// Struct representing the body in a Rekor LogEntry.
struct Body {
  static void RegisterJSONConverter(base::JSONValueConverter<Body>* converter);

  std::string api_version;
  std::string kind;
  Spec spec;
};

// Convenient struct for verifying the `signed_entry_timestamp` in a Rekor
// LogEntry.
//
// This bundle can be verified using the public key from Rekor. The public key
// can be obtained from the `/api/v1/log/publicKey` Rest API. For
// `sigstore.dev`, it is a PEM-encoded x509/PKIX public key.
struct COMPONENT_EXPORT(DEVICE_FIDO) RekorSignatureBundle {
  RekorSignatureBundle(std::vector<uint8_t> canonicalized,
                       std::vector<uint8_t> signature);
  RekorSignatureBundle();
  ~RekorSignatureBundle();
  RekorSignatureBundle(const RekorSignatureBundle& rekor_signature_bundle);

  // Canonicalized JSON representation, based on RFC 8785 rules, of a subset
  // of a Rekor LogEntry fields that are signed to generate
  // `signed_entry_timestamp` (also a field in the Rekor LogEntry). These
  // fields include body, integratedTime, logID and logIndex.
  std::vector<uint8_t> canonicalized;
  // The signature over the canonicalized JSON document.
  std::vector<uint8_t> signature;
};

// Verifies a Rekor LogEntry. This includes verifying:
//
// 1. the signature in `signed_entry_timestamp` using Rekor's public key,
// 1. the signature in `body.spec.generic_signature` using the endorser's public
//    key,
// 1. that the content of the body equals `endorsement`.
bool COMPONENT_EXPORT(DEVICE_FIDO)
    VerifyRekorLogEntry(base::span<const uint8_t> log_entry,
                        base::span<const uint8_t> rekor_public_key,
                        base::span<const uint8_t> endorsement);

// Parses the given bytes into a Rekor `LogEntry` object.
std::optional<LogEntry> COMPONENT_EXPORT(DEVICE_FIDO)
    GetRekorLogEntry(base::span<const uint8_t> log_entry);

// Parses the given bytes into a Rekor `LogEntry` object, and returns its
// `body` parsed into an instance of `Body`.
std::optional<Body> COMPONENT_EXPORT(DEVICE_FIDO)
    GetRekorLogEntryBody(base::span<const uint8_t> log_entry);

// Parses a blob into a Rekor log entry and verifies the signature in
// `signed_entry_timestamp` using Rekor's public key.
bool COMPONENT_EXPORT(DEVICE_FIDO)
    VerifyRekorSignature(base::span<const uint8_t> log_entry,
                         base::span<const uint8_t> rekor_public_key);

// Verifies the signature in the body over the contents.
bool COMPONENT_EXPORT(DEVICE_FIDO)
    VerifyRekorBody(const Body& body, base::span<const uint8_t> contents_bytes);

// Parses `RekorSignatureBundle` from `log_entry`.
std::optional<RekorSignatureBundle> COMPONENT_EXPORT(DEVICE_FIDO)
    GetRekorSignatureBundle(const LogEntry& log_entry);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_REKOR_H_
