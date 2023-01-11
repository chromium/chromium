// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_XR_DEVICE_SERVICE_H_
#define CONTENT_BROWSER_XR_SERVICE_XR_DEVICE_SERVICE_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Acquires a remote handle to the sandboxed isolated XR Device Service
// instance, launching a process to host the service if necessary.
const CONTENT_EXPORT mojo::Remote<device::mojom::XRDeviceService>&
GetXRDeviceService();

// Acquires a pending remote handle to the device service host and posts
// a message to the io thread task runner to bind the host.
CONTENT_EXPORT mojo::PendingRemote<device::mojom::XRDeviceServiceHost>
CreateXRDeviceServiceHost();

void CONTENT_EXPORT SetXRDeviceServiceStartupCallbackForTestingInternal(
    base::RepeatingClosure callback);
}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_XR_DEVICE_SERVICE_H_
