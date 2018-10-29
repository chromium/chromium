// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "storage/browser/quota/storage_observer.h"

namespace storage {

// StorageObserver::Filter

StorageObserver::Filter::Filter()
    : storage_type(blink::mojom::StorageType::kUnknown) {}

StorageObserver::Filter::Filter(blink::mojom::StorageType storage_type,
                                const url::Origin& origin)
    : storage_type(storage_type), origin(origin) {}

bool StorageObserver::Filter::operator==(const Filter& other) const {
  return storage_type == other.storage_type &&
         origin == other.origin;
}

// StorageObserver::MonitorParams

StorageObserver::MonitorParams::MonitorParams()
    : dispatch_initial_state(false) {
}

StorageObserver::MonitorParams::MonitorParams(
    blink::mojom::StorageType storage_type,
    const url::Origin& origin,
    const base::TimeDelta& rate,
    bool get_initial_state)
    : filter(storage_type, origin),
      rate(rate),
      dispatch_initial_state(get_initial_state) {}

StorageObserver::MonitorParams::MonitorParams(
    const Filter& filter,
    const base::TimeDelta& rate,
    bool get_initial_state)
        : filter(filter),
          rate(rate),
          dispatch_initial_state(get_initial_state) {
}

// StorageObserver::Event

StorageObserver::Event::Event()
    : usage(0), quota(0) {
}

StorageObserver::Event::Event(const Filter& filter,
                              int64_t usage,
                              int64_t quota)
    : filter(filter), usage(usage), quota(quota) {}

bool StorageObserver::Event::operator==(const Event& other) const {
  return filter == other.filter &&
         usage == other.usage &&
         quota == other.quota;
}

}  // namespace storage
