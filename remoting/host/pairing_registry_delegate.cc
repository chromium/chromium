// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"

namespace remoting {

using protocol::PairingRegistry;

scoped_refptr<PairingRegistry> CreatePairingRegistry(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  scoped_refptr<PairingRegistry> pairing_registry;
  std::unique_ptr<PairingRegistry::Delegate> delegate(
      CreatePairingRegistryDelegate());
  if (delegate) {
    pairing_registry = new PairingRegistry(task_runner, std::move(delegate));
  }
  return pairing_registry;
}

}  // namespace remoting
