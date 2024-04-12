// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/opt_record_rdata.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string_view>
#include <utility>

#include "base/big_endian.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "net/dns/public/dns_protocol.h"

namespace net {

namespace {
std::string SerializeEdeOpt(uint16_t info_code, std::string_view extra_text) {
  std::string buf(2 + extra_text.size(), '\0');

  auto writer = base::SpanWriter(base::as_writable_byte_span(buf));
  CHECK(writer.WriteU16BigEndian(info_code));
  CHECK(writer.Write(base::as_byte_span(extra_text)));
  CHECK_EQ(writer.remaining(), 0u);
  return buf;
}
}  // namespace

OptRecordRdata::Opt::Opt(std::string data) : data_(std::move(data)) {}

bool OptRecordRdata::Opt::operator==(const OptRecordRdata::Opt& other) const {
  return IsEqual(other);
}

bool OptRecordRdata::Opt::operator!=(const OptRecordRdata::Opt& other) const {
  return !IsEqual(other);
}

bool OptRecordRdata::Opt::IsEqual(const OptRecordRdata::Opt& other) const {
  return GetCode() == other.GetCode() && data() == other.data();
}

OptRecordRdata::EdeOpt::EdeOpt(uint16_t info_code, std::string extra_text)
    : Opt(SerializeEdeOpt(info_code, extra_text)),
      info_code_(info_code),
      extra_text_(std::move(extra_text)) {
  CHECK(base::IsStringUTF8(extra_text_));
}

OptRecordRdata::EdeOpt::~EdeOpt() = default;

std::unique_ptr<OptRecordRdata::EdeOpt> OptRecordRdata::EdeOpt::Create(
    std::string data) {
  uint16_t info_code;
  auto edeReader = base::SpanReader(base::as_byte_span(data));

  // size must be at least 2: info_code + optional extra_text
  base::span<const uint8_t> extra_text;
  if (!edeReader.ReadU16BigEndian(info_code) ||
      !base::OptionalUnwrapTo(edeReader.Read(edeReader.remaining()),
                              extra_text)) {
    return nullptr;
  }

  if (!base::IsStringUTF8(base::as_string_view(extra_text))) {
    return nullptr;
  }

  return std::make_unique<EdeOpt>(
      info_code, std::string(base::as_string_view(extra_text)));
}

uint16_t OptRecordRdata::EdeOpt::GetCode() const {
  return EdeOpt::kOptCode;
}

OptRecordRdata::EdeOpt::EdeInfoCode
OptRecordRdata::EdeOpt::GetEnumFromInfoCode() const {
  return GetEnumFromInfoCode(info_code_);
}

OptRecordRdata::EdeOpt::EdeInfoCode OptRecordRdata::EdeOpt::GetEnumFromInfoCode(
    uint16_t info_code) {
  switch (info_code) {
    case 0:
      return EdeInfoCode::kOtherError;
    case 1:
      return EdeInfoCode::kUnsupportedDnskeyAlgorithm;
    case 2:
      return EdeInfoCode::kUnsupportedDsDigestType;
    case 3:
      return EdeInfoCode::kStaleAnswer;
    case 4:
      return EdeInfoCode::kForgedAnswer;
    case 5:
      return EdeInfoCode::kDnssecIndeterminate;
    case 6:
      return EdeInfoCode::kDnssecBogus;
    case 7:
      return EdeInfoCode::kSignatureExpired;
    case 8:
      return EdeInfoCode::kSignatureNotYetValid;
    case 9:
      return EdeInfoCode::kDnskeyMissing;
    case 10:
      return EdeInfoCode::kRrsigsMissing;
    case 11:
      return EdeInfoCode::kNoZoneKeyBitSet;
    case 12:
      return EdeInfoCode::kNsecMissing;
    case 13:
      return EdeInfoCode::kCachedError;
    case 14:
      return EdeInfoCode::kNotReady;
    case 15:
      return EdeInfoCode::kBlocked;
    case 16:
      return EdeInfoCode::kCensored;
    case 17:
      return EdeInfoCode::kFiltered;
    case 18:
      return EdeInfoCode::kProhibited;
    case 19:
      return EdeInfoCode::kStaleNxdomainAnswer;
    case 20:
      return EdeInfoCode::kNotAuthoritative;
    case 21:
      return EdeInfoCode::kNotSupported;
    case 22:
      return EdeInfoCode::kNoReachableAuthority;
    case 23:
      return EdeInfoCode::kNetworkError;
    case 24:
      return EdeInfoCode::kInvalidData;
    case 25:
      return EdeInfoCode::kSignatureExpiredBeforeValid;
    case 26:
      return EdeInfoCode::kTooEarly;
    case 27:
      return EdeInfoCode::kUnsupportedNsec3IterationsValue;
    default:
      return EdeInfoCode::kUnrecognizedErrorCode;
  }
}

OptRecordRdata::PaddingOpt::PaddingOpt(std::string padding)
    : Opt(std::move(padding)) {}

OptRecordRdata::PaddingOpt::PaddingOpt(uint16_t padding_len)
    : Opt(std::string(base::checked_cast<size_t>(padding_len), '\0')) {}

OptRecordRdata::PaddingOpt::~PaddingOpt() = default;

uint16_t OptRecordRdata::PaddingOpt::GetCode() const {
  return PaddingOpt::kOptCode;
}

OptRecordRdata::UnknownOpt::~UnknownOpt() = default;

std::unique_ptr<OptRecordRdata::UnknownOpt>
OptRecordRdata::UnknownOpt::CreateForTesting(uint16_t code, std::string data) {
  CHECK_IS_TEST();
  return base::WrapUnique(
      new OptRecordRdata::UnknownOpt(code, std::move(data)));
}

OptRecordRdata::UnknownOpt::UnknownOpt(uint16_t code, std::string data)
    : Opt(std::move(data)), code_(code) {
  CHECK(!base::Contains(kOptsWithDedicatedClasses, code));
}

uint16_t OptRecordRdata::UnknownOpt::GetCode() const {
  return code_;
}

OptRecordRdata::OptRecordRdata() = default;

OptRecordRdata::~OptRecordRdata() = default;

bool OptRecordRdata::operator==(const OptRecordRdata& other) const {
  return IsEqual(&other);
}

bool OptRecordRdata::operator!=(const OptRecordRdata& other) const {
  return !IsEqual(&other);
}

// static
std::unique_ptr<OptRecordRdata> OptRecordRdata::Create(std::string_view data) {
  auto rdata = std::make_unique<OptRecordRdata>();
  rdata->buf_.assign(data.begin(), data.end());

  auto reader = base::SpanReader(base::as_byte_span(data));
  while (reader.remaining() > 0u) {
    uint16_t opt_code, opt_data_size;
    base::span<const uint8_t> opt_data;

    if (!reader.ReadU16BigEndian(opt_code) ||
        !reader.ReadU16BigEndian(opt_data_size) ||
        !base::OptionalUnwrapTo(reader.Read(opt_data_size), opt_data)) {
      return nullptr;
    }

    // After the Opt object has been parsed, parse the contents (the data)
    // depending on the opt_code. The specific Opt subclasses all inherit from
    // Opt. If an opt code does not have a matching Opt subclass, a simple Opt
    // object will be created, and data won't be parsed.

    std::unique_ptr<Opt> opt;

    switch (opt_code) {
      case dns_protocol::kEdnsPadding:
        opt = std::make_unique<OptRecordRdata::PaddingOpt>(
            std::string(base::as_string_view(opt_data)));
        break;
      case dns_protocol::kEdnsExtendedDnsError:
        opt = OptRecordRdata::EdeOpt::Create(
            std::string(base::as_string_view(opt_data)));
        break;
      default:
        opt = base::WrapUnique(new OptRecordRdata::UnknownOpt(
            opt_code, std::string(base::as_string_view(opt_data))));
        break;
    }

    // Confirm that opt is not null, which would be the result of a failed
    // parse.
    if (!opt) {
      return nullptr;
    }

    rdata->opts_.emplace(opt_code, std::move(opt));
  }

  return rdata;
}

uint16_t OptRecordRdata::Type() const {
  return OptRecordRdata::kType;
}

bool OptRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type()) {
    return false;
  }
  const OptRecordRdata* opt_other = static_cast<const OptRecordRdata*>(other);
  return opt_other->buf_ == buf_;
}

void OptRecordRdata::AddOpt(std::unique_ptr<Opt> opt) {
  std::string_view opt_data = opt->data();

  // Resize buffer to accommodate new OPT.
  const size_t orig_rdata_size = buf_.size();
  buf_.resize(orig_rdata_size + Opt::kHeaderSize + opt_data.size());

  // Start writing from the end of the existing rdata.
  auto writer = base::SpanWriter(base::as_writable_byte_span(buf_));
  CHECK(writer.Skip(orig_rdata_size));
  bool success = writer.WriteU16BigEndian(opt->GetCode()) &&
                 writer.WriteU16BigEndian(opt_data.size()) &&
                 writer.Write(base::as_byte_span(opt_data));
  DCHECK(success);

  opts_.emplace(opt->GetCode(), std::move(opt));
}

bool OptRecordRdata::ContainsOptCode(uint16_t opt_code) const {
  return base::Contains(opts_, opt_code);
}

std::vector<const OptRecordRdata::Opt*> OptRecordRdata::GetOpts() const {
  std::vector<const OptRecordRdata::Opt*> opts;
  opts.reserve(OptCount());
  for (const auto& elem : opts_) {
    opts.push_back(elem.second.get());
  }
  return opts;
}

std::vector<const OptRecordRdata::PaddingOpt*> OptRecordRdata::GetPaddingOpts()
    const {
  std::vector<const OptRecordRdata::PaddingOpt*> opts;
  auto range = opts_.equal_range(dns_protocol::kEdnsPadding);
  for (auto it = range.first; it != range.second; ++it) {
    opts.push_back(static_cast<const PaddingOpt*>(it->second.get()));
  }
  return opts;
}

std::vector<const OptRecordRdata::EdeOpt*> OptRecordRdata::GetEdeOpts() const {
  std::vector<const OptRecordRdata::EdeOpt*> opts;
  auto range = opts_.equal_range(dns_protocol::kEdnsExtendedDnsError);
  for (auto it = range.first; it != range.second; ++it) {
    opts.push_back(static_cast<const EdeOpt*>(it->second.get()));
  }
  return opts;
}

}  // namespace net
