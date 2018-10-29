// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto packet = base::MakeRefCounted<net::IOBufferWithSize>(size);
  memcpy(packet->data(), data, size);
  base::Optional<net::DnsQuery> query;
  query.emplace(packet);
  if (!query->Parse(size)) {
    return 0;
  }
  net::DnsResponse response(query->id(), true /* is_authoritative */,
                            {} /* answers */, {} /* additional records */,
                            query);
  std::string out =
      base::HexEncode(response.io_buffer()->data(), response.io_buffer_size());
  return 0;
}
