// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CONNECTION_MIGRATION_INFORMATION_H_
#define NET_BASE_CONNECTION_MIGRATION_INFORMATION_H_

#include <stdint.h>

#include "net/base/net_export.h"

namespace net {

// Keeps track of connection migration related information.
// TODO(crbug.com/379516116): Expand the fields to connection migration
// related information as well.
struct NET_EXPORT ConnectionMigrationInformation {
  // Keeps track of the count for network event that occurred.
  struct NET_EXPORT_PRIVATE NetworkEventCount {
    NetworkEventCount() = default;
    NetworkEventCount(uint32_t default_network_change,
                      uint32_t network_disconnected,
                      uint32_t network_connected,
                      uint32_t path_degrading);

    bool operator==(const ConnectionMigrationInformation::NetworkEventCount&
                        other) const = default;
    ConnectionMigrationInformation::NetworkEventCount operator-(
        const ConnectionMigrationInformation::NetworkEventCount& other) const;

    uint32_t default_network_changed_num = 0;
    uint32_t network_disconnected_num = 0;
    uint32_t network_connected_num = 0;
    uint32_t path_degrading_num = 0;
  };

  ConnectionMigrationInformation() = default;
  explicit ConnectionMigrationInformation(
      ConnectionMigrationInformation::NetworkEventCount event);

  bool operator==(const ConnectionMigrationInformation& other) const = default;
  bool operator!=(const ConnectionMigrationInformation& other) const = default;
  ConnectionMigrationInformation operator-(
      const ConnectionMigrationInformation& other) const;

  NetworkEventCount event_count;
};

}  // namespace net

#endif  // NET_BASE_CONNECTION_MIGRATION_INFORMATION_H_
