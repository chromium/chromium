// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/https_record_rdata.h"

#include <stdint.h>

#include <memory>

#include "base/check.h"
#include "base/strings/string_piece.h"
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
    CHECK(!alias->alias_name().empty());
  } else {
    ServiceFormHttpsRecordRdata* service = parsed->AsServiceForm();
    CHECK_GT(service->priority(), 0);
    service->service_name();
    service->alpn_ids();
    service->default_alpn();
    service->port();
    service->ipv4_hint();
    service->ech_config();
    service->ipv6_hint();
    service->unparsed_params();
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  ParseAndExercise(
      base::StringPiece(reinterpret_cast<const char*>(data), size));
  return 0;
}

}  // namespace net
