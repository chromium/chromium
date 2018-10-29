// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_STORAGE_OBSERVER_H_
#define STORAGE_BROWSER_QUOTA_STORAGE_OBSERVER_H_

#include <stdint.h>

#include "base/time/time.h"
#include "storage/browser/quota/quota_client.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

// This interface is implemented by observers that wish to monitor storage
// events, such as changes in quota or usage.
class STORAGE_EXPORT StorageObserver {
 public:
  struct STORAGE_EXPORT Filter {
    // The storage type to monitor. This must not be kUnknown or
    // kQuotaNotManaged.
    blink::mojom::StorageType storage_type;

    // The origin to monitor usage for. Must be specified.
    url::Origin origin;

    Filter();
    Filter(blink::mojom::StorageType storage_type, const url::Origin& origin);
    bool operator==(const Filter& other) const;
  };

  struct STORAGE_EXPORT MonitorParams {
    // Storage type and origin to monitor.
    Filter filter;

    // The rate at which storage events will be fired. Events will be fired at
    // approximately this rate, or when a storage status change has been
    // detected, whichever is the least frequent.
    base::TimeDelta rate;

    // If set to true, the observer will be dispatched an event when added.
    bool dispatch_initial_state;

    MonitorParams();
    MonitorParams(blink::mojom::StorageType storage_type,
                  const url::Origin& origin,
                  const base::TimeDelta& rate,
                  bool get_initial_state);
    MonitorParams(const Filter& filter,
                  const base::TimeDelta& rate,
                  bool get_initial_state);
  };

  struct STORAGE_EXPORT Event {
    // The storage type and origin monitored.
    Filter filter;

    // The current usage corresponding to the filter.
    int64_t usage;

    // The quota corresponding to the filter.
    int64_t quota;

    Event();
    Event(const Filter& filter, int64_t usage, int64_t quota);
    bool operator==(const Event& other) const;
  };

  // Will be called on the IO thread when a storage event occurs.
  virtual void OnStorageEvent(const Event& event) = 0;

 protected:
  virtual ~StorageObserver() {}
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_STORAGE_OBSERVER_H_
