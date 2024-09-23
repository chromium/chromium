// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string_view>

#include "net/base/address_list.h"
#include "net/base/ip_address.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string_view hostname(reinterpret_cast<const char*>(data), size);
  net::IPAddress address;

  if (net::ParseURLHostnameToAddress(hostname, &address)) {
    // To fuzz port number without spending raw bytes of data, use hash(data).
    std::size_t data_hash = std::hash<std::string>()(std::string(hostname));
    uint16_t port = static_cast<uint16_t>(data_hash & 0xFFFF);
    net::AddressList addresses =
        net::AddressList::CreateFromIPAddress(address, port);

    for (const auto& endpoint : addresses) {
      endpoint.ToString();
    }
  }

  return 0;
}
