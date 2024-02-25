// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_logging_client.h"

#include "base/logging.h"
#include "base/observer_list.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

FakeFlossLoggingClient::FakeFlossLoggingClient() = default;

FakeFlossLoggingClient::~FakeFlossLoggingClient() = default;

void FakeFlossLoggingClient::IsDebugEnabled(ResponseCallback<bool> callback) {
  std::move(callback).Run(debug_enabled_);
}

void FakeFlossLoggingClient::SetDebugLogging(ResponseCallback<Void> callback,
                                             bool enabled) {
  debug_enabled_ = enabled;
  std::move(callback).Run(floss::Void());
}

void FakeFlossLoggingClient::Init(dbus::Bus* bus,
                                  const std::string& service_name,
                                  const int adapter_index,
                                  base::Version version,
                                  base::OnceClosure on_ready) {
  version_ = version;
  std::move(on_ready).Run();
}

}  // namespace floss
