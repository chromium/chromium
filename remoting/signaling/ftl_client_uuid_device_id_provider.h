// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_CLIENT_UUID_DEVICE_ID_PROVIDER_H_
#define REMOTING_SIGNALING_FTL_CLIENT_UUID_DEVICE_ID_PROVIDER_H_

#include <string>

#include "remoting/signaling/ftl_device_id_provider.h"

namespace remoting {

// A device ID provider using ephemeral client UUIDs. A random UUID will be
// generated on construction.
class FtlClientUuidDeviceIdProvider : public FtlDeviceIdProvider {
 public:
  FtlClientUuidDeviceIdProvider();
  ~FtlClientUuidDeviceIdProvider() override;

  ftl::DeviceId GetDeviceId() override;

 private:
  std::string client_uuid_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_CLIENT_UUID_DEVICE_ID_PROVIDER_H_
