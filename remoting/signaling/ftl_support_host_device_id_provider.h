// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_SUPPORT_HOST_DEVICE_ID_PROVIDER_H_
#define REMOTING_SIGNALING_FTL_SUPPORT_HOST_DEVICE_ID_PROVIDER_H_

#include <string>

#include "remoting/signaling/ftl_device_id_provider.h"

namespace remoting {

// A device ID provider for remote support hosts.
class FtlSupportHostDeviceIdProvider : public FtlDeviceIdProvider {
 public:
  explicit FtlSupportHostDeviceIdProvider(const std::string&);
  ~FtlSupportHostDeviceIdProvider() override;

  ftl::DeviceId GetDeviceId() override;

 private:
  std::string support_host_device_id_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_SUPPORT_HOST_DEVICE_ID_PROVIDER_H_
