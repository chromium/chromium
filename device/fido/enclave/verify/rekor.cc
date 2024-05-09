// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
LogEntry::LogEntry(LogEntry& log_entry) = default;

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
  std::string canonicalized =
      base::StrCat({"{\"body\":{\"", log_entry.body, "\"},\"integratedTime\":{",
                    base::NumberToString(log_entry.integrated_time.ToTimeT()),
                    "},\"logID\":{\"", log_entry.log_id, "\"},\"logIndex\":{",
                    base::NumberToString(log_entry.log_index), "}}"});
  RekorSignatureBundle rekor_signature_bundle;
  rekor_signature_bundle.canonicalized =
      std::vector<uint8_t>(canonicalized.begin(), canonicalized.end());
  rekor_signature_bundle.signature =
      std::vector<uint8_t>(signature.begin(), signature.end());
  return rekor_signature_bundle;
}

}  // namespace device::enclave
