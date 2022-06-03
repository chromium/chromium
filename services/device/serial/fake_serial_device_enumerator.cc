// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/fake_serial_device_enumerator.h"

#include <utility>


namespace device {

FakeSerialEnumerator::FakeSerialEnumerator() = default;

FakeSerialEnumerator::~FakeSerialEnumerator() = default;

void FakeSerialEnumerator::AddDevicePath(const base::FilePath& path) {
  auto port = mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->path = path;
  paths_[path] = port->token;
  AddPort(std::move(port));
}

void FakeSerialEnumerator::RemoveDevicePath(const base::FilePath& path) {
  auto it = paths_.find(path);
  DCHECK(it != paths_.end());
  base::UnguessableToken token = it->second;
  paths_.erase(it);
  RemovePort(token);
}

}  // namespace device
