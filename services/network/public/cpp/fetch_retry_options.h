// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FETCH_RETRY_OPTIONS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FETCH_RETRY_OPTIONS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "services/network/public/mojom/fetch_retry_options.mojom-shared.h"

namespace network {

// This implements a data structure holding the policy configurations for fetch
// retry feature.
//
// This struct is needed so that we can pass the same object when plumbing.
struct COMPONENT_EXPORT(NETWORK_CPP_FETCH_RETRY_OPTIONS) FetchRetryOptions {
  FetchRetryOptions();
  ~FetchRetryOptions();

  FetchRetryOptions(FetchRetryOptions&&);
  FetchRetryOptions& operator=(FetchRetryOptions&&);

  FetchRetryOptions(const FetchRetryOptions&);
  FetchRetryOptions& operator=(const FetchRetryOptions&);

  bool operator==(const FetchRetryOptions&) const;

  uint32_t max_attempts = 0;
  std::optional<base::TimeDelta> initial_delay;
  std::optional<double> backoff_factor;
  std::optional<base::TimeDelta> max_age;
  bool retry_after_unload = false;
  bool retry_non_idempotent = false;
  bool retry_only_if_server_unreached = false;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FETCH_RETRY_OPTIONS_H_
