// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/connection_migration_information.h"

#include <stdint.h>

namespace net {

ConnectionMigrationInformation::NetworkEventCount::NetworkEventCount(
    uint32_t default_network_change,
    uint32_t network_disconnected,
    uint32_t network_connected,
    uint32_t path_degrading)
    : default_network_changed_num(default_network_change),
      network_disconnected_num(network_disconnected),
      network_connected_num(network_connected),
      path_degrading_num(path_degrading) {}

ConnectionMigrationInformation::NetworkEventCount
ConnectionMigrationInformation::NetworkEventCount::operator-(
    const ConnectionMigrationInformation::NetworkEventCount& other) const {
  return ConnectionMigrationInformation::NetworkEventCount(
      default_network_changed_num - other.default_network_changed_num,
      network_disconnected_num - other.network_disconnected_num,
      network_connected_num - other.network_connected_num,
      path_degrading_num - other.path_degrading_num);
}

ConnectionMigrationInformation::ConnectionMigrationInformation(
    ConnectionMigrationInformation::NetworkEventCount event)
    : event_count(event) {}

ConnectionMigrationInformation ConnectionMigrationInformation::operator-(
    const ConnectionMigrationInformation& other) const {
  return ConnectionMigrationInformation(event_count - other.event_count);
}

}  // namespace net
