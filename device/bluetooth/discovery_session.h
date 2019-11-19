// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DISCOVERY_SESSION_H_
#define DEVICE_BLUETOOTH_DISCOVERY_SESSION_H_

#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"

namespace bluetooth {

// Implementation of Mojo DiscoverySession in
// device/bluetooth/public/mojom/adapter.mojom.
// It handles requests to interact with a DiscoverySession.
// Uses the platform abstraction of device/bluetooth.
// An instance of this class is constructed by Adapter and strongly bound
// to its MessagePipe. When the instance is destroyed, the underlying
// BluetoothDiscoverySession is requested to stop. Users are encouraged to call
// DiscoverySession::Stop in order to respond to a failed request to stop
// device discovery.
class DiscoverySession : public mojom::DiscoverySession {
 public:
  explicit DiscoverySession(
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  ~DiscoverySession() override;

  // mojom::DiscoverySession overrides:
  void IsActive(IsActiveCallback callback) override;
  void Stop(StopCallback callback) override;

 private:
  void OnStop(StopCallback callback);
  void OnStopError(StopCallback callback);

  // The underlying discovery session.
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  base::WeakPtrFactory<DiscoverySession> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DiscoverySession);
};

}  // namespace bluetooth

#endif
