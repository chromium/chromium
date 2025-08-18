// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_COLLECTOR_CONFIG_H_
#define SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_COLLECTOR_CONFIG_H_

#include <stdint.h>

#include "base/component_export.h"

namespace network {

// DevtoolsDurableMessageCollectorConfig holds information about the configured
// network body storage.
struct COMPONENT_EXPORT(NETWORK_SERVICE) DevtoolsDurableMessageCollectorConfig {
  int64_t max_storage_size = 0u;
};

}  // namespace network

#endif  // SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_COLLECTOR_CONFIG_H_
