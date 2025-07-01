// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_GAMEPAD_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_GAMEPAD_SHARED_H_

#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace device {
class Gamepads;
}  // namespace device

namespace ppapi {

// TODO(brettw) when we remove the non-IPC-based gamepad implementation, this
// code should all move into the GamepadResource.
PPAPI_SHARED_EXPORT void ConvertDeviceGamepadData(
    const device::Gamepads& device_data,
    PP_GamepadsSampleData* output_data);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_GAMEPAD_SHARED_H_
