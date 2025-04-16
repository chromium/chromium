// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/reconnect_event_observer_mojom_traits.h"

#include "mojo/public/cpp/bindings/message.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/mojom/reconnect_event_observer.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<network::mojom::ConnectionKeepAliveConfigDataView,
                  net::ConnectionKeepAliveConfig>::
    Read(network::mojom::ConnectionKeepAliveConfigDataView data,
         net::ConnectionKeepAliveConfig* out) {
  // Check if `ping_interval_sec` is smaller than the `idle_timeout_sec`
  // so that we will be able to send at least one ping before closing the
  // connection due to the idle timeout.
  if (data.ping_interval_in_seconds() > data.idle_timeout_in_seconds()) {
    return false;
  }

  out->ping_interval_in_seconds = data.ping_interval_in_seconds();
  out->idle_timeout_in_seconds = data.idle_timeout_in_seconds();
  return true;
}

}  // namespace mojo
