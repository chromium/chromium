// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_parsed.h"

#include <utility>

#include "base/logging.h"
#include "net/dns/dns_response.h"
#include "net/dns/record_rdata.h"

namespace net {

RecordParsed::RecordParsed(const std::string& name,
                           uint16_t type,
                           uint16_t klass,
                           uint32_t ttl,
                           std::unique_ptr<const RecordRdata> rdata,
                           base::Time time_created)
    : name_(name),
      type_(type),
      klass_(klass),
      ttl_(ttl),
      rdata_(std::move(rdata)),
      time_created_(time_created) {}

RecordParsed::~RecordParsed() = default;

// static
std::unique_ptr<const RecordParsed> RecordParsed::CreateFrom(
    DnsRecordParser* parser,
    base::Time time_created) {
  DnsResourceRecord record;
  std::unique_ptr<const RecordRdata> rdata;

  if (!parser->ReadRecord(&record))
    return std::unique_ptr<const RecordParsed>();

  switch (record.type) {
    case ARecordRdata::kType:
      rdata = ARecordRdata::Create(record.rdata, *parser);
      break;
    case AAAARecordRdata::kType:
      rdata = AAAARecordRdata::Create(record.rdata, *parser);
      break;
    case CnameRecordRdata::kType:
      rdata = CnameRecordRdata::Create(record.rdata, *parser);
      break;
    case PtrRecordRdata::kType:
      rdata = PtrRecordRdata::Create(record.rdata, *parser);
      break;
    case SrvRecordRdata::kType:
      rdata = SrvRecordRdata::Create(record.rdata, *parser);
      break;
    case TxtRecordRdata::kType:
      rdata = TxtRecordRdata::Create(record.rdata, *parser);
      break;
    case NsecRecordRdata::kType:
      rdata = NsecRecordRdata::Create(record.rdata, *parser);
      break;
    case OptRecordRdata::kType:
      rdata = OptRecordRdata::Create(record.rdata, *parser);
      break;
    case EsniRecordRdata::kType:
      rdata = EsniRecordRdata::Create(record.rdata, *parser);
      break;
    default:
      DVLOG(1) << "Unknown RData type for received record: " << record.type;
      return std::unique_ptr<const RecordParsed>();
  }

  if (!rdata.get())
    return std::unique_ptr<const RecordParsed>();

  return std::unique_ptr<const RecordParsed>(
      new RecordParsed(record.name, record.type, record.klass, record.ttl,
                       std::move(rdata), time_created));
}

bool RecordParsed::IsEqual(const RecordParsed* other, bool is_mdns) const {
  DCHECK(other);
  uint16_t klass = klass_;
  uint16_t other_klass = other->klass_;

  if (is_mdns) {
    klass &= dns_protocol::kMDnsClassMask;
    other_klass &= dns_protocol::kMDnsClassMask;
  }

  return name_ == other->name_ &&
      klass == other_klass &&
      type_ == other->type_ &&
      rdata_->IsEqual(other->rdata_.get());
}

}  // namespace net
