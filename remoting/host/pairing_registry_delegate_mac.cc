// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate.h"

#include "base/task/task_runner.h"

namespace remoting {

using protocol::PairingRegistry;

std::unique_ptr<PairingRegistry::Delegate> CreatePairingRegistryDelegate() {
  return nullptr;
}

}  // namespace remoting
