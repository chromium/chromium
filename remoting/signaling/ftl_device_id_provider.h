// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_DEVICE_ID_PROVIDER_H_
#define REMOTING_SIGNALING_FTL_DEVICE_ID_PROVIDER_H_

#include "remoting/proto/ftl/v1/ftl_messages.pb.h"

namespace remoting {

// Class that provides device ID to be used to sign in for FTL.
class FtlDeviceIdProvider {
 public:
  virtual ~FtlDeviceIdProvider() = default;

  // Gets a device ID to use for signing into FTL. It's the subclass'
  // responsibility to store and reuse stored device ID.
  //
  // Subclasses should consider adding a prefix to the device ID, like
  // "crd-win-host-".
  virtual ftl::DeviceId GetDeviceId() = 0;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_DEVICE_ID_PROVIDER_H_
