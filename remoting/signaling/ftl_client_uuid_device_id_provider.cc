// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_client_uuid_device_id_provider.h"

#include "base/guid.h"

namespace remoting {

FtlClientUuidDeviceIdProvider::FtlClientUuidDeviceIdProvider()
    : client_uuid_(base::GenerateGUID()) {}

FtlClientUuidDeviceIdProvider::~FtlClientUuidDeviceIdProvider() = default;

ftl::DeviceId FtlClientUuidDeviceIdProvider::GetDeviceId() {
  ftl::DeviceId device_id;
  device_id.set_type(ftl::DeviceIdType_Type_CLIENT_UUID);
  device_id.set_id(client_uuid_);
  return device_id;
}

}  // namespace remoting
