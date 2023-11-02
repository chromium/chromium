// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_host_device_id_provider.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace remoting {

namespace {

#if BUILDFLAG(IS_WIN)
constexpr char kDeviceIdPrefix[] = "crd-win-host-";
#elif BUILDFLAG(IS_APPLE)
constexpr char kDeviceIdPrefix[] = "crd-mac-host-";
#elif BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kDeviceIdPrefix[] = "crd-cros-host-";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
