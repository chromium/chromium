// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/shared_impl/ppb_gamepad_shared.h"

#include <cstring>

#include "device/gamepad/public/cpp/gamepads.h"

namespace ppapi {

void ConvertDeviceGamepadData(const device::Gamepads& device_data,
                              PP_GamepadsSampleData* output_data) {
  output_data->length = device::Gamepads::kItemsLengthCap;
  for (unsigned i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    PP_GamepadSampleData& output_pad = output_data->items[i];
    const device::Gamepad& device_pad = device_data.items[i];
    output_pad.connected = device_pad.connected ? PP_TRUE : PP_FALSE;
    if (device_pad.connected) {
      static_assert(sizeof(output_pad.id) == sizeof(device_pad.id),
                    "id size does not match");
      std::memcpy(output_pad.id, device_pad.id, sizeof(output_pad.id));
      output_pad.timestamp = static_cast<double>(device_pad.timestamp);
      output_pad.axes_length = device_pad.axes_length;
      for (unsigned j = 0; j < device_pad.axes_length; ++j)
        output_pad.axes[j] = static_cast<float>(device_pad.axes[j]);
      output_pad.buttons_length = device_pad.buttons_length;
      for (unsigned j = 0; j < device_pad.buttons_length; ++j)
        output_pad.buttons[j] = static_cast<float>(device_pad.buttons[j].value);
    }
  }
}

}  // namespace ppapi
