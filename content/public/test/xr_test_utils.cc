// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/xr_test_utils.h"

#include "content/browser/xr/service/xr_device_service.h"

namespace content {

const mojo::Remote<device::mojom::XRDeviceService>&
GetXRDeviceServiceForTesting() {
  return GetXRDeviceService();
}

void SetXRDeviceServiceStartupCallbackForTesting(
    base::RepeatingClosure callback) {
  SetXRDeviceServiceStartupCallbackForTestingInternal(std::move(callback));
}

}  // namespace content
