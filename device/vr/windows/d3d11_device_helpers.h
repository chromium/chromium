// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_WINDOWS_D3D11_DEVICE_HELPERS_H_
#define DEVICE_VR_WINDOWS_D3D11_DEVICE_HELPERS_H_

#include <dxgi.h>
#include <wrl.h>
#include <cstdint>

namespace vr {

void GetD3D11_1Adapter(int32_t* adapter_index, IDXGIAdapter** adapter);
void GetD3D11_1AdapterIndex(int32_t* adapter_index);

}  // namespace vr

#endif  // DEVICE_VR_WINDOWS_D3D11_DEVICE_HELPERS_H_
