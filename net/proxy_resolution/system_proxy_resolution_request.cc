// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/system_proxy_resolution_request.h"

#include <utility>

#include "base/check.h"
#include "net/log/net_log_event_type.h"
#include "net/proxy_resolution/system_proxy_resolution_service.h"

namespace net {

SystemProxyResolutionRequest::SystemProxyResolutionRequest(
    SystemProxyResolutionService* service,
    GURL url,
    std::string method,
    NetworkAnonymizationKey network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback user_callback,
    const NetLogWithSource& net_log)
    : service_(service),
      user_callback_(std::move(user_callback)),
      results_(results),
      url_(std::move(url)),
      method_(std::move(method)),
      network_anonymization_key_(std::move(network_anonymization_key)),
      net_log_(net_log),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK(!user_callback_.is_null());
}

SystemProxyResolutionRequest::~SystemProxyResolutionRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (service_) {
    // At this point the derived sub-object is already destroyed (C++ destructor
    // ordering). RemovePendingRequest uses |this| only as a pointer value for
    // set lookup — no virtual dispatch or derived member access occurs.
    service_->RemovePendingRequest(this);
    net_log_.AddEvent(NetLogEventType::CANCELLED);

    // Note: platform subclass destructor runs first and is responsible for
    // cancelling the platform-specific resolver request (e.g., resetting
    // the WinHTTP resolver handle). C++ destructor ordering guarantees
    // derived-first, so platform cleanup happens before we get here.

    net_log_.EndEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
  }
}

void SystemProxyResolutionRequest::MarkCompleted() {
  DCHECK(service_);
  service_->RemovePendingRequest(this);
  service_ = nullptr;
}

LoadState SystemProxyResolutionRequest::GetLoadState() const {
  // TODO(crbug.com/40111093): Consider adding a LoadState for "We're
  // waiting on system APIs to do their thing".
  return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
}

}  // namespace net
