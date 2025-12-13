// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_NETWORK_CONDITIONS_H_
#define SERVICES_NETWORK_THROTTLING_NETWORK_CONDITIONS_H_

#include "base/component_export.h"
#include "base/unguessable_token.h"

namespace network {

// NetworkConditions holds information about desired network conditions.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkConditions {
 public:
  NetworkConditions();

  NetworkConditions(const NetworkConditions&);
  NetworkConditions& operator=(const NetworkConditions&);
  bool operator==(const NetworkConditions&) const = default;

  ~NetworkConditions();

  explicit NetworkConditions(bool offline);
  NetworkConditions(bool offline,
                    double latency,
                    double download_throughput,
                    double upload_throughput);
  NetworkConditions(bool offline,
                    double latency,
                    double download_throughput,
                    double upload_throughput,
                    double packet_loss,
                    int packet_queue_length,
                    bool packet_reordering,
                    std::optional<base::UnguessableToken> rule_id);

  bool IsThrottling() const;

  bool offline() const { return offline_; }
  const std::optional<base::UnguessableToken>& rule_id() const {
    return rule_id_;
  }

  // These are 0 if the corresponding throttle is disabled, >0 otherwise.
  double latency() const { return latency_; }
  double download_throughput() const { return download_throughput_; }
  double upload_throughput() const { return upload_throughput_; }
  double packet_loss() const { return packet_loss_; }
  int packet_queue_length() const { return packet_queue_length_; }
  bool packet_reordering() const { return packet_reordering_; }

 private:
  bool offline_;
  double latency_;
  double download_throughput_;
  double upload_throughput_;
  double packet_loss_;
  int packet_queue_length_;
  bool packet_reordering_;
  std::optional<base::UnguessableToken> rule_id_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_NETWORK_CONDITIONS_H_
