// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_id.h"

#include <tuple>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "net/nqe/proto/network_id_proto.pb.h"

namespace net::nqe::internal {

// static
NetworkID NetworkID::FromString(const std::string& network_id) {
  std::string base64_decoded;
  if (!base::Base64Decode(network_id, &base64_decoded)) {
    return NetworkID(NetworkChangeNotifier::CONNECTION_UNKNOWN, std::string(),
                     INT32_MIN);
  }

  NetworkIDProto network_id_proto;
  if (!network_id_proto.ParseFromString(base64_decoded)) {
    return NetworkID(NetworkChangeNotifier::CONNECTION_UNKNOWN, std::string(),
                     INT32_MIN);
  }

  return NetworkID(static_cast<NetworkChangeNotifier::ConnectionType>(
                       network_id_proto.connection_type()),
                   network_id_proto.id(), network_id_proto.signal_strength());
}

NetworkID::NetworkID(NetworkChangeNotifier::ConnectionType type,
                     const std::string& id,
                     int32_t signal_strength)
    : type(type), id(id), signal_strength(signal_strength) {
  // A valid value of |signal_strength| must be between 0 and 4 (both
  // inclusive).
  DCHECK((0 <= signal_strength && 4 >= signal_strength) ||
         (INT32_MIN == signal_strength));
}

NetworkID::NetworkID(const NetworkID& other) = default;

NetworkID::~NetworkID() = default;

bool NetworkID::operator==(const NetworkID& other) const {
  return type == other.type && id == other.id &&
         signal_strength == other.signal_strength;
}

bool NetworkID::operator!=(const NetworkID& other) const {
  return !operator==(other);
}

NetworkID& NetworkID::operator=(const NetworkID& other) = default;

// Overloaded to support ordered collections.
bool NetworkID::operator<(const NetworkID& other) const {
  return std::tie(type, id, signal_strength) <
         std::tie(other.type, other.id, other.signal_strength);
}

std::string NetworkID::ToString() const {
  NetworkIDProto network_id_proto;
  network_id_proto.set_connection_type(static_cast<int>(type));
  network_id_proto.set_id(id);
  network_id_proto.set_signal_strength(signal_strength);

  std::string serialized_network_id;
  if (!network_id_proto.SerializeToString(&serialized_network_id))
    return "";

  return base::Base64Encode(serialized_network_id);
}

}  // namespace net::nqe::internal
