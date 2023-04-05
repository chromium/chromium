// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SUBRESOURCE_LOAD_METRICS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SUBRESOURCE_LOAD_METRICS_H_

namespace blink {

// Subresource load data to be used for the metrics.
struct SubresourceLoadMetrics {
  // The total number of sub resource loads except for ResourceType::kRaw
  // in the frame.
  // Note that this includes subresources where a service worker responded.
  uint32_t number_of_subresources_loaded = 0;
  // The number of sub resource loads that a service worker fetch handler
  // called `respondWith`. i.e. no fallback to network.
  uint32_t number_of_subresource_loads_handled_by_service_worker = 0;
  // Whether a pervasive payload (aka sub resource) was requested. Once this is
  // set as true, it will be true for all future updates in a page load.
  bool pervasive_payload_requested = false;
  // Number of bytes fetched by the network for pervasive payloads on a page.
  int64_t pervasive_bytes_fetched = 0;
  // Total number of bytes fetched by the network.
  int64_t total_bytes_fetched = 0;

  bool operator==(const SubresourceLoadMetrics& other) const {
    return number_of_subresources_loaded ==
               other.number_of_subresources_loaded &&
           number_of_subresource_loads_handled_by_service_worker ==
               other.number_of_subresource_loads_handled_by_service_worker &&
           pervasive_payload_requested == other.pervasive_payload_requested &&
           pervasive_bytes_fetched == other.pervasive_bytes_fetched &&
           total_bytes_fetched == other.total_bytes_fetched;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SUBRESOURCE_LOAD_METRICS_H_
