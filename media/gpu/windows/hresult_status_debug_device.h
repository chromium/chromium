// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_HRESULT_STATUS_DEBUG_DEVICE_H_
#define MEDIA_GPU_WINDOWS_HRESULT_STATUS_DEBUG_DEVICE_H_

#include <d3d11.h>
#include <wrl/client.h>

#include "media/base/status.h"
#include "media/gpu/windows/d3d11_com_defs.h"

namespace media {

// In debug mode, this uses |AddDebugMessages()| to give us a detailed error
// trace from the d3d11 stack. Otherwise, it just generates a Status with the
// kWindowsD3D11Error code.
Status D3D11HresultToStatus(
    HRESULT hresult,
    ComD3D11Device device,
    const char* message = nullptr,
    const base::Location& location = base::Location::Current());

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_HRESULT_STATUS_DEBUG_DEVICE_H_