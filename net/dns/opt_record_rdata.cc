// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/opt_record_rdata.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "base/big_endian.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "net/base/io_buffer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

OptRecordRdata::OptRecordRdata() = default;

OptRecordRdata::OptRecordRdata(OptRecordRdata&& other) = default;

OptRecordRdata::~OptRecordRdata() = default;

OptRecordRdata& OptRecordRdata::operator=(OptRecordRdata&& other) = default;

// static
std::unique_ptr<OptRecordRdata> OptRecordRdata::Create(
    const base::StringPiece& data,
    const DnsRecordParser& parser) {
  auto rdata = base::WrapUnique(new OptRecordRdata());
  rdata->buf_.assign(data.begin(), data.end());

  auto reader = base::BigEndianReader::FromStringPiece(data);
  while (reader.remaining() > 0) {
    uint16_t opt_code, opt_data_size;
    base::StringPiece opt_data;

    if (!(reader.ReadU16(&opt_code) && reader.ReadU16(&opt_data_size) &&
          reader.ReadPiece(&opt_data, opt_data_size))) {
      return nullptr;
    }

    rdata->opts_.emplace(opt_code, Opt(opt_code, opt_data));
  }

  return rdata;
}

uint16_t OptRecordRdata::Type() const {
  return OptRecordRdata::kType;
}

bool OptRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type())
    return false;
  const OptRecordRdata* opt_other = static_cast<const OptRecordRdata*>(other);
  return opt_other->opts_ == opts_;
}

void OptRecordRdata::AddOpt(const Opt& opt) {
  base::StringPiece opt_data = opt.data();

  // Resize buffer to accommodate new OPT.
  const size_t orig_rdata_size = buf_.size();
  buf_.resize(orig_rdata_size + Opt::kHeaderSize + opt_data.size());

  // Start writing from the end of the existing rdata.
  base::BigEndianWriter writer(buf_.data() + orig_rdata_size, buf_.size());
  bool success = writer.WriteU16(opt.code()) &&
                 writer.WriteU16(opt_data.size()) &&
                 writer.WriteBytes(opt_data.data(), opt_data.size());
  DCHECK(success);

  opts_.emplace(opt.code(), opt);
}

void OptRecordRdata::AddOpts(const OptRecordRdata& other) {
  buf_.insert(buf_.end(), other.buf_.begin(), other.buf_.end());
  opts_.insert(other.opts_.begin(), other.opts_.end());
}

bool OptRecordRdata::ContainsOptCode(uint16_t opt_code) const {
  return opts_.find(opt_code) != opts_.end();
}

OptRecordRdata::Opt::Opt(uint16_t code, base::StringPiece data)
    : code_(code), data_(data) {}

bool OptRecordRdata::Opt::operator==(const OptRecordRdata::Opt& other) const {
  return code_ == other.code_ && data_ == other.data_;
}

}  // namespace net
