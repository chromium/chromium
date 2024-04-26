// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_fuchsia.h"

#include "base/notreached.h"
#include "services/device/hid/hid_connection.h"

namespace device {

HidServiceFuchsia::HidServiceFuchsia() = default;
HidServiceFuchsia::~HidServiceFuchsia() = default;

void HidServiceFuchsia::Connect(const std::string& device_id,
                                bool allow_protected_reports,
                                bool allow_fido_reports,
                                ConnectCallback callback) {
  // TODO(crbug.com/42050450): Implement this.
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run(nullptr);
}

base::WeakPtr<HidService> HidServiceFuchsia::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
