// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cros/discovery.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace device {

FidoChromeOSDiscovery::FidoChromeOSDiscovery(
    base::RepeatingCallback<uint32_t()> generate_request_id_callback)
    : FidoDiscoveryBase(FidoTransportProtocol::kInternal),
      authenticator_(std::make_unique<ChromeOSAuthenticator>(
          std::move(generate_request_id_callback))),
      weak_factory_(this) {}

FidoChromeOSDiscovery::~FidoChromeOSDiscovery() {}

void FidoChromeOSDiscovery::Start() {
  DCHECK(!authenticator_);
  if (!observer()) {
    return;
  }

  // Start() is currently invoked synchronously in the
  // FidoRequestHandler ctor. Invoke AddAuthenticator() asynchronously
  // to avoid hairpinning FidoRequestHandler::AuthenticatorAdded()
  // before the request handler has an observer.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FidoChromeOSDiscovery::AddAuthenticator,
                                weak_factory_.GetWeakPtr()));
}

void FidoChromeOSDiscovery::AddAuthenticator() {
  observer()->DiscoveryStarted(this, /*success=*/true, {authenticator_.get()});
}

}  // namespace device
