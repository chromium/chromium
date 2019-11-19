// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_RECORD_PARSED_H_
#define NET_DNS_RECORD_PARSED_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

class DnsRecordParser;
class RecordRdata;

// Parsed record. This is a form of DnsResourceRecord where the rdata section
// has been parsed into a data structure.
class NET_EXPORT_PRIVATE RecordParsed {
 public:
  virtual ~RecordParsed();

  // All records are inherently immutable. Return a const pointer.
  static std::unique_ptr<const RecordParsed> CreateFrom(
      DnsRecordParser* parser,
      base::Time time_created);

  const std::string& name() const { return name_; }
  uint16_t type() const { return type_; }
  uint16_t klass() const { return klass_; }
  uint32_t ttl() const { return ttl_; }

  base::Time time_created() const { return time_created_; }

  template <class T> const T* rdata() const {
    if (T::kType != type_)
      return nullptr;
    return static_cast<const T*>(rdata_.get());
  }

  // Check if two records have the same data. Ignores time_created and ttl.
  // If |is_mdns| is true, ignore the top bit of the class
  // (the cache flush bit).
  bool IsEqual(const RecordParsed* other, bool is_mdns) const;

 private:
  RecordParsed(const std::string& name,
               uint16_t type,
               uint16_t klass,
               uint32_t ttl,
               std::unique_ptr<const RecordRdata> rdata,
               base::Time time_created);

  std::string name_;  // in dotted form
  const uint16_t type_;
  const uint16_t klass_;
  const uint32_t ttl_;

  const std::unique_ptr<const RecordRdata> rdata_;

  const base::Time time_created_;
};

}  // namespace net

#endif  // NET_DNS_RECORD_PARSED_H_
