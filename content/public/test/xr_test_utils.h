// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_XR_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_XR_TEST_UTILS_H_

#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Acquires a remote handle to the sandboxed isolated XR Device Service
// instance, launching a process to host the service if necessary.
// This is really just a wrapper for |GetXRDeviceService| which is only exposed
// internally to content/browser.
const mojo::Remote<device::mojom::XRDeviceService>&
GetXRDeviceServiceForTesting();

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_XR_TEST_UTILS_H_
