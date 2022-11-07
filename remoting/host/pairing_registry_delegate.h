// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_H_
#define REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "remoting/protocol/pairing_registry.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {
// Returns a platform-specific pairing registry delegate that will save to
// permanent storage. Returns nullptr on platforms that don't support pairing.
std::unique_ptr<protocol::PairingRegistry::Delegate>
CreatePairingRegistryDelegate();

// Convenience function which returns a new PairingRegistry, using the delegate
// returned by CreatePairingRegistryDelegate(). The passed |task_runner| is used
// to run the delegate's methods asynchronously.
scoped_refptr<protocol::PairingRegistry> CreatePairingRegistry(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

}  // namespace remoting

#endif  // REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_H_
