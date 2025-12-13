// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_dns_prober.h"

#include "base/functional/callback.h"
#include "net/base/address_list.h"

namespace content {

PrefetchDNSProber::PrefetchDNSProber(OnDNSResultsCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(callback_);
}

PrefetchDNSProber::~PrefetchDNSProber() {
  if (callback_) {
    // Indicates some kind of mojo error. Play it safe and return no success.
    std::move(callback_).Run(net::ERR_FAILED, {});
  }
}

void PrefetchDNSProber::OnComplete(
    int32_t error,
    const net::ResolveErrorInfo& resolve_error_info,
    const net::AddressList& resolved_addresses,
    const net::HostResolverEndpointResults& alternative_endpoints) {
  if (callback_) {
    std::move(callback_).Run(error, resolved_addresses);
  }
}

}  // namespace content
