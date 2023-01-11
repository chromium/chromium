// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "net/dns/dns_config.h"
#include "net/log/net_log.h"

namespace net {

DnsSession::DnsSession(const DnsConfig& config,
                       const RandIntCallback& rand_int_callback,
                       NetLog* net_log)
    : config_(config),
      rand_callback_(base::BindRepeating(rand_int_callback,
                                         0,
                                         std::numeric_limits<uint16_t>::max())),
      net_log_(net_log) {}

DnsSession::~DnsSession() = default;

uint16_t DnsSession::NextQueryId() const {
  return static_cast<uint16_t>(rand_callback_.Run());
}

void DnsSession::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace net
