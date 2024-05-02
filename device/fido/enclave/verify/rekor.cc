// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/rekor.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
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

}  // namespace device::enclave
