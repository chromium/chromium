// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_message.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace device {

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> span) {
  constexpr size_t kHidPacketSize = 64;

  auto packet = span.first(std::min(kHidPacketSize, span.size()));
  auto msg = FidoHidMessage::CreateFromSerializedData(
      std::vector<uint8_t>(packet.begin(), packet.end()));
  if (!msg)
    return 0;

  span = span.subspan(packet.size());
  while (!span.empty()) {
    packet = span.first(std::min(kHidPacketSize, span.size()));
    msg->AddContinuationPacket(
        std::vector<uint8_t>(packet.begin(), packet.end()));
    span = span.subspan(packet.size());
  }
  return 0;
}

}  // namespace device
