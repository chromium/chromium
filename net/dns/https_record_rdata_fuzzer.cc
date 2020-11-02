// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/https_record_rdata.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "net/base/ip_address.h"
#include "net/dns/public/dns_protocol.h"

namespace net {
namespace {

void ParseAndExercise(base::StringPiece data) {
  std::unique_ptr<HttpsRecordRdata> parsed = HttpsRecordRdata::Parse(data);
  std::unique_ptr<HttpsRecordRdata> parsed2 = HttpsRecordRdata::Parse(data);

  CHECK_EQ(!!parsed, !!parsed2);

  if (!parsed)
    return;

  CHECK(parsed->IsEqual(parsed.get()));
  CHECK(parsed->IsEqual(parsed2.get()));

  CHECK_EQ(parsed->Type(), dns_protocol::kTypeHttps);
  if (parsed->IsAlias()) {
    AliasFormHttpsRecordRdata* alias = parsed->AsAliasForm();
    alias->alias_name();
  } else {
    ServiceFormHttpsRecordRdata* service = parsed->AsServiceForm();
    CHECK_GT(service->priority(), 0);
    service->service_name();
    service->alpn_ids();
    service->default_alpn();
    service->port();
    service->ech_config();
    service->unparsed_params();
    service->IsCompatible();

    std::set<uint16_t> mandatory_keys = service->mandatory_keys();
    CHECK(mandatory_keys.find(dns_protocol::kHttpsServiceParamKeyMandatory) ==
          mandatory_keys.end());

    std::vector<IPAddress> ipv4_hint = service->ipv4_hint();
    for (const IPAddress& address : ipv4_hint) {
      CHECK(address.IsIPv4());
    }

    std::vector<IPAddress> ipv6_hint = service->ipv6_hint();
    for (const IPAddress& address : ipv6_hint) {
      CHECK(address.IsIPv6());
    }
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ParseAndExercise(
      base::StringPiece(reinterpret_cast<const char*>(data), size));
  return 0;
}

}  // namespace net
