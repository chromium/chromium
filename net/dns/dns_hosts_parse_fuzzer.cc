// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "net/dns/dns_hosts.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  net::DnsHosts dns_hosts;
  net::ParseHostsWithCommaModeForTesting(input, &dns_hosts,
                                         net::PARSE_HOSTS_COMMA_IS_TOKEN);
  dns_hosts.clear();
  net::ParseHostsWithCommaModeForTesting(input, &dns_hosts,
                                         net::PARSE_HOSTS_COMMA_IS_WHITESPACE);
  return 0;
}
