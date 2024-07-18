// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "device/fido/hid/fido_hid_message.h"

namespace device {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kHidPacketSize = 64;
  auto span = base::make_span(data, size);

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
