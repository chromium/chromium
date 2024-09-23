// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/enclave/verify/rekor.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "device/fido/enclave/verify/utils.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device::enclave {

namespace {

bool ConvertIntToTime(const base::Value* s, base::Time* result) {
  if (!s->is_int()) {
    return false;
  }
  *result = base::Time::FromTimeT(s->GetInt());
  return true;
}

bool ParseOptionalLogEntryVerification(
    const base::Value* s,
    std::optional<LogEntryVerification>* result) {
  LogEntryVerification log_entry_verification;
  base::JSONValueConverter<LogEntryVerification> converter;
  if (!converter.Convert(*s, &log_entry_verification)) {
    *result = std::nullopt;
    return false;
  }
  *result = std::move(log_entry_verification);
  return true;
}

bool IntToUint64t(const base::Value* s, uint64_t* result) {
  if (!s->is_int() || s->GetInt() < 0) {
    return false;
  }
  *result = static_cast<uint64_t>(s->GetInt());
  return true;
}

bool ConvertToBytes(const base::Value* s, std::vector<uint8_t>* result) {
  if (!s->is_string()) {
    return false;
  }
  std::vector<uint8_t> output(s->GetString().begin(), s->GetString().end());
  *result = std::move(output);
  return true;
}

bool CheckHashType(const base::Value* s, HashType* result) {
  if (!s->is_string()) {
    return false;
  }
  if (s->GetString() != "sha256") {
    return false;
  }
  *result = kSHA256;
  return true;
}

}  // namespace

RekorSignatureBundle::RekorSignatureBundle(std::vector<uint8_t> canonicalized,
                                           std::vector<uint8_t> signature)
    : canonicalized(std::move(canonicalized)),
      signature(std::move(signature)) {}
RekorSignatureBundle::RekorSignatureBundle() = default;
RekorSignatureBundle::~RekorSignatureBundle() = default;
RekorSignatureBundle::RekorSignatureBundle(
    const RekorSignatureBundle& rekor_signature_bundle) = default;

LogEntry::LogEntry(std::string body,
                   base::Time integrated_time,
                   std::string log_id,
                   uint64_t log_index,
                   std::optional<LogEntryVerification> verification)
    : body(std::move(body)),
      integrated_time(std::move(integrated_time)),
      log_id(std::move(log_id)),
      log_index(log_index),
      verification(std::move(verification)) {}
LogEntry::LogEntry() = default;
LogEntry::~LogEntry() = default;
LogEntry::LogEntry(const LogEntry& log_entry) = default;
LogEntry::LogEntry(LogEntry&& log_entry) = default;

Hash::Hash(std::vector<uint8_t> bytes, HashType hash_type)
    : bytes(std::move(bytes)), hash_type(hash_type) {}
Hash::Hash() = default;
Hash::~Hash() = default;
Hash::Hash(const Hash& hash) = default;

void Hash::RegisterJSONConverter(base::JSONValueConverter<Hash>* converter) {
  converter->RegisterCustomValueField("algorithm", &Hash::hash_type,
                                      &CheckHashType);
  converter->RegisterCustomValueField("value", &Hash::bytes, &ConvertToBytes);
}

void LogEntry::RegisterJSONConverter(
    base::JSONValueConverter<LogEntry>* converter) {
  converter->RegisterStringField("body", &LogEntry::body);
  converter->RegisterCustomValueField<base::Time>(
      "integratedTime", &LogEntry::integrated_time, &ConvertIntToTime);
  converter->RegisterStringField("logID", &LogEntry::log_id);
  converter->RegisterCustomValueField("logIndex", &LogEntry::log_index,
                                      &IntToUint64t);
  converter->RegisterCustomValueField<std::optional<LogEntryVerification>>(
      "verification", &LogEntry::verification,
      &ParseOptionalLogEntryVerification);
}

void LogEntryVerification::RegisterJSONConverter(
    base::JSONValueConverter<LogEntryVerification>* converter) {
  converter->RegisterStringField("signedEntryTimestamp",
                                 &LogEntryVerification::signed_entry_timestamp);
}

void PublicKey::RegisterJSONConverter(
    base::JSONValueConverter<PublicKey>* converter) {
  converter->RegisterStringField("content", &PublicKey::content);
}

void GenericSignature::RegisterJSONConverter(
    base::JSONValueConverter<GenericSignature>* converter) {
  converter->RegisterStringField("content", &GenericSignature::content);
  converter->RegisterStringField("format", &GenericSignature::format);
  converter->RegisterNestedField("publicKey", &GenericSignature::public_key);
}

void Data::RegisterJSONConverter(base::JSONValueConverter<Data>* converter) {
  converter->RegisterNestedField("hash", &Data::hash);
}

void Spec::RegisterJSONConverter(base::JSONValueConverter<Spec>* converter) {
  converter->RegisterNestedField("data", &Spec::data);
  converter->RegisterNestedField("signature", &Spec::generic_signature);
}

void Body::RegisterJSONConverter(base::JSONValueConverter<Body>* converter) {
  converter->RegisterStringField("apiVersion", &Body::api_version);
  converter->RegisterStringField("kind", &Body::kind);
  converter->RegisterNestedField("spec", &Body::spec);
}

std::optional<LogEntry> GetRekorLogEntry(base::span<const uint8_t> log_entry) {
  std::string_view json = reinterpret_cast<const char*>(log_entry.data());
  std::optional<base::Value> log_entry_json = base::JSONReader::Read(json);
  if (!log_entry_json.has_value()) {
    return std::nullopt;
  }
  LogEntry log_entry_result;
  base::JSONValueConverter<LogEntry> converter;
  if (!converter.Convert(log_entry_json.value(), &log_entry_result)) {
    return std::nullopt;
  }
  return log_entry_result;
}

std::optional<Body> GetRekorLogEntryBody(base::span<const uint8_t> log_entry) {
  std::optional<LogEntry> log_entry_output = GetRekorLogEntry(log_entry);
  if (!log_entry_output.has_value()) {
    return std::nullopt;
  }
  std::string body;
  if (!base::Base64Decode(log_entry_output->body, &body)) {
    return std::nullopt;
  }
  std::optional<base::Value> body_json = base::JSONReader::Read(body);
  if (!body_json.has_value()) {
    return std::nullopt;
  }
  Body body_result;
  base::JSONValueConverter<Body> converter;
  if (!converter.Convert(body_json.value(), &body_result)) {
    return std::nullopt;
  }
  return body_result;
}

std::optional<RekorSignatureBundle> GetRekorSignatureBundle(
    const LogEntry& log_entry) {
  if (!log_entry.verification.has_value()) {
    return std::nullopt;
  }
  std::string signature;
  if (!base::Base64Decode(log_entry.verification->signed_entry_timestamp,
                          &signature)) {
    return std::nullopt;
  }
  if (log_entry.body.find('\\') != std::string::npos ||
      log_entry.log_id.find('\\') != std::string::npos ||
      log_entry.body.find('\"') != std::string::npos ||
      log_entry.log_id.find('\"') != std::string::npos) {
    return std::nullopt;
  }
  std::string canonicalized = base::StrCat(
      {"{\"body\":\"", log_entry.body, "\",\"integratedTime\":",
       base::NumberToString(log_entry.integrated_time.ToTimeT()),
       ",\"logID\":\"", log_entry.log_id,
       "\",\"logIndex\":", base::NumberToString(log_entry.log_index), "}"});
  RekorSignatureBundle rekor_signature_bundle;
  rekor_signature_bundle.canonicalized =
      std::vector<uint8_t>(canonicalized.begin(), canonicalized.end());
  rekor_signature_bundle.signature =
      std::vector<uint8_t>(signature.begin(), signature.end());
  return rekor_signature_bundle;
}

bool VerifyRekorSignature(base::span<const uint8_t> log_entry,
                          base::span<const uint8_t> rekor_public_key) {
  std::optional<LogEntry> log_entry_result = GetRekorLogEntry(log_entry);
  if (!log_entry_result.has_value()) {
    return false;
  }
  std::optional<RekorSignatureBundle> rekor_signature_bundle =
      GetRekorSignatureBundle(log_entry_result.value());
  if (!rekor_signature_bundle.has_value()) {
    return false;
  }
  return VerifySignatureRaw(rekor_signature_bundle->signature,
                            rekor_signature_bundle->canonicalized,
                            rekor_public_key)
      .has_value();
}

bool VerifyRekorBody(const Body& body,
                     base::span<const uint8_t> contents_bytes) {
  if (body.spec.generic_signature.format != "x509" ||
      body.spec.data.hash.hash_type != kSHA256) {
    return false;
  }
  uint8_t contents_hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(contents_bytes.data()),
         contents_bytes.size(), contents_hash);
  std::string contents_hash_hex = base::ToLowerASCII(
      base::HexEncode(contents_hash, std::size(contents_hash)));
  std::string_view bytes_str(
      reinterpret_cast<const char*>(body.spec.data.hash.bytes.data()),
      body.spec.data.hash.bytes.size());
  if (contents_hash_hex != bytes_str) {
    return false;
  }
  std::string signature;
  if (!base::Base64Decode(body.spec.generic_signature.content, &signature)) {
    return false;
  }
  std::string public_key_pem;
  if (!base::Base64Decode(body.spec.generic_signature.public_key.content,
                          &public_key_pem)) {
    return false;
  }
  auto public_key = ConvertPemToRaw(public_key_pem);
  if (!public_key.has_value()) {
    return false;
  }
  return VerifySignatureRaw(
             base::make_span(static_cast<uint8_t*>((uint8_t*)signature.data()),
                             signature.size()),
             contents_bytes, public_key.value())
      .has_value();
}

bool VerifyRekorLogEntry(base::span<const uint8_t> log_entry,
                         base::span<const uint8_t> rekor_public_key,
                         base::span<const uint8_t> endorsement) {
  if (!VerifyRekorSignature(log_entry, rekor_public_key)) {
    return false;
  }
  std::optional<Body> body = GetRekorLogEntryBody(log_entry);
  if (!body.has_value()) {
    return false;
  }
  return VerifyRekorBody(body.value(), endorsement);
}

}  // namespace device::enclave
