// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_logging_client.h"

namespace floss {

FlossLoggingClient::FlossLoggingClient() = default;
FlossLoggingClient::~FlossLoggingClient() = default;

// static
std::unique_ptr<FlossLoggingClient> FlossLoggingClient::Create() {
  return std::make_unique<FlossLoggingClient>();
}

void FlossLoggingClient::IsDebugEnabled(ResponseCallback<bool> callback) {
  CallAdapterLoggingMethod<bool>(std::move(callback),
                                 adapter_logging::kIsDebugEnabled);
}

// Sets debug logging on this adapter. Changes will take effect immediately.
void FlossLoggingClient::SetDebugLogging(ResponseCallback<Void> callback,
                                         bool enabled) {
  CallAdapterLoggingMethod<Void>(std::move(callback),
                                 adapter_logging::kSetDebugLogging, enabled);
}

// Initializes the logging client with given adapter.
void FlossLoggingClient::Init(dbus::Bus* bus,
                              const std::string& service_name,
                              const int adapter_index,
                              base::Version version,
                              base::OnceClosure on_ready) {
  bus_ = bus;
  service_name_ = service_name;
  logging_path_ = GenerateLoggingPath(adapter_index);
  version_ = version;

  if (!bus_->GetObjectProxy(service_name_, logging_path_)) {
    LOG(ERROR) << "FlossLoggingClient couldn't init. Object proxy was null.";
    return;
  }

  std::move(on_ready).Run();
}

}  // namespace floss
