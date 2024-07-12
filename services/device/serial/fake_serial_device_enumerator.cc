// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/fake_serial_device_enumerator.h"

#include <utility>

#include "base/not_fatal_until.h"

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
  CHECK(it != paths_.end(), base::NotFatalUntil::M130);
  base::UnguessableToken token = it->second;
  paths_.erase(it);
  RemovePort(token);
}

}  // namespace device
