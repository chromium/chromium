// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_dns_prober.h"

#include <optional>

#include "base/functional/callback.h"
#include "net/base/address_list.h"
#include "net/dns/public/resolve_error_info.h"

namespace content {

PrefetchDNSProber::PrefetchDNSProber(OnDNSResultsCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(callback_);
}

PrefetchDNSProber::~PrefetchDNSProber() {
  if (callback_) {
    // Indicates some kind of mojo error. Play it safe and return no success.
    std::move(callback_).Run(net::ERR_FAILED, std::nullopt);
  }
}

void PrefetchDNSProber::OnComplete(
    int32_t error,
    const net::ResolveErrorInfo& resolve_error_info,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  if (callback_) {
    std::move(callback_).Run(error, resolved_addresses);
  }
}

}  // namespace content