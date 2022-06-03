// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "device/fido/cable/fido_ble_frames.h"
#include "device/fido/fido_constants.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* raw_data, size_t size) {
  auto data_span = base::make_span(raw_data, size);
  std::vector<uint8_t> data(data_span.begin(), data_span.end());

  {
    device::FidoBleFrameInitializationFragment fragment(
        device::FidoBleDeviceCommand::kMsg, 21123, data_span);
    std::vector<uint8_t> buffer;
    fragment.Serialize(&buffer);

    device::FidoBleFrameInitializationFragment parsed_fragment;
    device::FidoBleFrameInitializationFragment::Parse(data, &parsed_fragment);
    device::FidoBleFrameInitializationFragment::Parse(buffer, &parsed_fragment);

    buffer.clear();
    parsed_fragment.Serialize(&buffer);
  }

  {
    device::FidoBleFrameContinuationFragment fragment(data_span, 61);
    std::vector<uint8_t> buffer;
    fragment.Serialize(&buffer);

    device::FidoBleFrameContinuationFragment parsed_fragment;
    device::FidoBleFrameContinuationFragment::Parse(data, &parsed_fragment);
    device::FidoBleFrameContinuationFragment::Parse(buffer, &parsed_fragment);

    buffer.clear();
    parsed_fragment.Serialize(&buffer);
  }

  {
    device::FidoBleFrame frame(device::FidoBleDeviceCommand::kPing, data);
    auto fragments = frame.ToFragments(20);

    device::FidoBleFrameAssembler assembler(fragments.first);
    while (!fragments.second.empty()) {
      assembler.AddFragment(fragments.second.front());
      fragments.second.pop();
    }

    auto result_frame = std::move(*assembler.GetFrame());
    result_frame.command();
  }

  return 0;
}
