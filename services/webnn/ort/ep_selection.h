// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_EP_SELECTION_H_
#define SERVICES_WEBNN_ORT_EP_SELECTION_H_

#include "base/containers/span.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

// Returns a vector of selected execution provider devices for WebNN based on
// the specified device type.
// In this method, the input `available_devices` are first reordered using
// WebNN's custom sorting logic. Repeated calls with the same device set and the
// specified device type will return the same ordered devices, regardless of the
// input order of `available_devices`.
// At most 3 EP devices will be selected.
std::vector<const OrtEpDevice*> SelectEpDevicesForDeviceType(
    base::span<const OrtEpDevice* const> available_devices,
    mojom::Device device_type);

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_EP_SELECTION_H_
