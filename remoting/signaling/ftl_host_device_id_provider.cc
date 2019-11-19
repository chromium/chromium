// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_host_device_id_provider.h"

#include "build/build_config.h"

namespace remoting {

namespace {

#if defined(OS_WIN)
constexpr char kDeviceIdPrefix[] = "crd-win-host-";
#elif defined(OS_MACOSX)
constexpr char kDeviceIdPrefix[] = "crd-mac-host-";
#elif defined(OS_CHROMEOS)
constexpr char kDeviceIdPrefix[] = "crd-cros-host-";
#elif defined(OS_LINUX)
constexpr char kDeviceIdPrefix[] = "crd-linux-host-";
#else
constexpr char kDeviceIdPrefix[] = "crd-unknown-host-";
#endif

}  // namespace

FtlHostDeviceIdProvider::FtlHostDeviceIdProvider(const std::string& host_id) {
  device_id_.set_type(ftl::DeviceIdType_Type_CHROMOTING_HOST_ID);
  device_id_.set_id(kDeviceIdPrefix + host_id);
}

FtlHostDeviceIdProvider::~FtlHostDeviceIdProvider() = default;

ftl::DeviceId FtlHostDeviceIdProvider::GetDeviceId() {
  return device_id_;
}

}  // namespace remoting
