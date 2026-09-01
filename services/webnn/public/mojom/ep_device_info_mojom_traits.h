// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_EP_DEVICE_INFO_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_EP_DEVICE_INFO_MOJOM_TRAITS_H_

#include <optional>
#include <string>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/ep_device_info.h"
#include "services/webnn/public/mojom/ep_device_info.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::EpDeviceInfoDataView, webnn::EpDeviceInfo> {
  static const std::string& ep_name(const webnn::EpDeviceInfo& device) {
    return device.ep_name;
  }

  static webnn::mojom::Device device_type(const webnn::EpDeviceInfo& device) {
    return device.device_type;
  }

  static uint32_t device_id(const webnn::EpDeviceInfo& device) {
    return device.device_id;
  }

  static uint32_t vendor_id(const webnn::EpDeviceInfo& device) {
    return device.vendor_id;
  }

  static const std::string& target_architecture(
      const webnn::EpDeviceInfo& device) {
    return device.target_architecture;
  }

  static bool Read(webnn::mojom::EpDeviceInfoDataView data,
                   webnn::EpDeviceInfo* out) {
    if (!data.ReadEpName(&out->ep_name)) {
      return false;
    }
    out->device_type = data.device_type();
    out->device_id = data.device_id();
    out->vendor_id = data.vendor_id();
    if (!data.ReadTargetArchitecture(&out->target_architecture)) {
      return false;
    }
    if (!out->target_architecture.empty() &&
        !webnn::IsValidEpTargetArchitecture(out->target_architecture)) {
      return false;
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_EP_DEVICE_INFO_MOJOM_TRAITS_H_
