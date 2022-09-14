// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <vector>

#include "net/third_party/quiche/src/quiche/quic/core/crypto/transport_parameters.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  auto perspective = data_provider.ConsumeBool() ? quic::Perspective::IS_CLIENT
                                                 : quic::Perspective::IS_SERVER;
  quic::TransportParameters transport_parameters;
  std::vector<uint8_t> remaining_bytes =
      data_provider.ConsumeRemainingBytes<uint8_t>();
  quic::ParsedQuicVersion version = quic::AllSupportedVersionsWithTls().front();
  CHECK(version.UsesTls());
  std::string error_details;
  quic::ParseTransportParameters(version, perspective, remaining_bytes.data(),
                                 remaining_bytes.size(), &transport_parameters,
                                 &error_details);
  return 0;
}
