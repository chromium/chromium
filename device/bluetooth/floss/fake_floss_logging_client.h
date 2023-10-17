// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_LOGGING_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_LOGGING_CLIENT_H_

#include "base/logging.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_logging_client.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT FakeFlossLoggingClient
    : public FlossLoggingClient {
 public:
  FakeFlossLoggingClient();
  ~FakeFlossLoggingClient() override;

  // Testing functions.
  bool GetDebugEnabledForTesting() const { return debug_enabled_; }
  void SetDebugEnabledForTesting(bool enabled) { debug_enabled_ = enabled; }

  // Fake overrides.
  void IsDebugEnabled(ResponseCallback<bool> callback) override;
  void SetDebugLogging(ResponseCallback<Void> callback, bool enabled) override;
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

 private:
  bool debug_enabled_ = false;
  base::WeakPtrFactory<FakeFlossLoggingClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_LOGGING_CLIENT_H_
