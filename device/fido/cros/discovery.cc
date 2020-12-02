// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cros/discovery.h"

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace device {

FidoChromeOSDiscovery::FidoChromeOSDiscovery(
    base::RepeatingCallback<uint32_t()> generate_request_id_callback)
    : FidoDiscoveryBase(FidoTransportProtocol::kInternal),
      generate_request_id_callback_(generate_request_id_callback),
      weak_factory_(this) {}

FidoChromeOSDiscovery::~FidoChromeOSDiscovery() {}

void FidoChromeOSDiscovery::Start() {
  DCHECK(!authenticator_);
  if (!observer()) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(
          &ChromeOSAuthenticator::IsUVPlatformAuthenticatorAvailableBlocking),
      base::BindOnce(&FidoChromeOSDiscovery::MaybeAddAuthenticator,
                     weak_factory_.GetWeakPtr()));
}

void FidoChromeOSDiscovery::MaybeAddAuthenticator(bool is_available) {
  if (!is_available) {
    observer()->DiscoveryStarted(this, /*success=*/false);
    return;
  }
  authenticator_ =
      std::make_unique<ChromeOSAuthenticator>(generate_request_id_callback_);
  observer()->DiscoveryStarted(this, /*success=*/true, {authenticator_.get()});
}

}  // namespace device
