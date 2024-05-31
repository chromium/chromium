// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/https_record_rdata.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "net/base/ip_address.h"
#include "net/dns/public/dns_protocol.h"

namespace net {
namespace {

void ParseAndExercise(FuzzedDataProvider& data_provider) {
  std::string data1 = data_provider.ConsumeRandomLengthString();
  std::unique_ptr<HttpsRecordRdata> parsed = HttpsRecordRdata::Parse(data1);
  std::unique_ptr<HttpsRecordRdata> parsed2 = HttpsRecordRdata::Parse(data1);
  std::unique_ptr<HttpsRecordRdata> parsed3 =
      HttpsRecordRdata::Parse(data_provider.ConsumeRemainingBytesAsString());

  CHECK_EQ(!!parsed, !!parsed2);

  if (!parsed)
    return;

  // `parsed` and `parsed2` parsed from the same data, so they should always be
  // equal.
  CHECK(parsed->IsEqual(parsed.get()));
  CHECK(parsed->IsEqual(parsed2.get()));
  CHECK(parsed2->IsEqual(parsed.get()));

  // Attempt comparison with an rdata parsed from separate data. IsEqual() will
  // probably return false most of the time, but easily could be true if the
  // input data is similar enough.
  if (parsed3)
    CHECK_EQ(parsed->IsEqual(parsed3.get()), parsed3->IsEqual(parsed.get()));

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
    CHECK(!base::Contains(mandatory_keys,
                          dns_protocol::kHttpsServiceParamKeyMandatory));

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
  FuzzedDataProvider data_provider(data, size);
  ParseAndExercise(data_provider);
  return 0;
}

}  // namespace net
