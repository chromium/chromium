// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVICE_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_DEVICE_SERVICE_H_

#include "content/common/content_export.h"
#include "services/device/public/mojom/device_service.mojom.h"

namespace content {

// Returns the main interface to the browser's global in-process instance of the
// Device Service.
CONTENT_EXPORT device::mojom::DeviceService& GetDeviceService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVICE_SERVICE_H_
