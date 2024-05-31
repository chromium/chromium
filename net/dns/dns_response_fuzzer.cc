// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <optional>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"

namespace {

void ValidateParsedResponse(net::DnsResponse& response,
                            const net::IOBufferWithSize& packet,
                            std::optional<net::DnsQuery> query = std::nullopt) {
  CHECK_EQ(response.io_buffer(), &packet);
  CHECK_EQ(static_cast<int>(response.io_buffer_size()), packet.size());

  response.id();
  if (response.IsValid()) {
    CHECK(response.id().has_value());
    response.flags();
    response.rcode();

    CHECK_EQ(response.dotted_qnames().size(), response.question_count());
    CHECK_EQ(response.qtypes().size(), response.question_count());
    if (response.question_count() == 1) {
      response.GetSingleDottedName();
      response.GetSingleQType();
    }

    response.answer_count();
    response.authority_count();
    response.additional_answer_count();

    bool success = false;
    size_t last_offset = 0;
    net::DnsRecordParser parser = response.Parser();
    do {
      net::DnsResourceRecord record;
      success = parser.ReadRecord(&record);

      CHECK(!success || parser.GetOffset() > last_offset);
      last_offset = parser.GetOffset();
    } while (success);

    // Attempt to parse a couple more.
    for (int i = 0; i < 10; ++i) {
      net::DnsResourceRecord record;
      CHECK(!parser.ReadRecord(&record));
    }

    if (query) {
      CHECK_EQ(response.question_count(), 1u);
      CHECK_EQ(response.id().value(), query->id());
      std::optional<std::string> dotted_qname =
          net::dns_names_util::NetworkToDottedName(query->qname(),
                                                   /*require_complete=*/true);
      CHECK(dotted_qname.has_value());
      CHECK_EQ(response.GetSingleDottedName(), dotted_qname.value());
      CHECK_EQ(response.GetSingleQType(), query->qtype());
    }
  }
}

}  // namespace

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  std::string response_string = data_provider.ConsumeRandomLengthString();

  auto response_packet =
      base::MakeRefCounted<net::IOBufferWithSize>(response_string.size());
  memcpy(response_packet->data(), response_string.data(),
         response_string.size());

  net::DnsResponse received_response(response_packet, response_string.size());
  received_response.InitParseWithoutQuery(response_string.size());
  ValidateParsedResponse(received_response, *response_packet.get());

  size_t query_size = data_provider.remaining_bytes();
  auto query_packet = base::MakeRefCounted<net::IOBufferWithSize>(query_size);
  data_provider.ConsumeData(query_packet->data(), query_size);
  net::DnsQuery query(query_packet);

  if (!query.Parse(query_size))
    return 0;

  net::DnsResponse received_response_with_query(response_packet,
                                                response_string.size());
  received_response_with_query.InitParse(response_string.size(), query);
  ValidateParsedResponse(received_response_with_query, *response_packet.get(),
                         query);

  net::DnsResponse response(query.id(), true /* is_authoritative */,
                            {} /* answers */, {} /* authority_records */,
                            {} /* additional records */, query);
  std::string out =
      base::HexEncode(response.io_buffer()->data(), response.io_buffer_size());

  return 0;
}
