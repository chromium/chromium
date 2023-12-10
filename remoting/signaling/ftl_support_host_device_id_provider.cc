// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_support_host_device_id_provider.h"

namespace remoting {

namespace {
constexpr char kDeviceIdPrefix[] = "crd-it2me-host-";
}

FtlSupportHostDeviceIdProvider::FtlSupportHostDeviceIdProvider(
    const std::string& device_id)
    : support_host_device_id_(kDeviceIdPrefix + device_id) {}

FtlSupportHostDeviceIdProvider::~FtlSupportHostDeviceIdProvider() = default;

ftl::DeviceId FtlSupportHostDeviceIdProvider::GetDeviceId() {
  ftl::DeviceId device_id;
  device_id.set_type(ftl::DeviceIdType_Type_CHROMOTING_HOST_ID);
  device_id.set_id(support_host_device_id_);
  return device_id;
}

}  // namespace remoting
