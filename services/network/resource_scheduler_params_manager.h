// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESOURCE_SCHEDULER_PARAMS_MANAGER_H_
#define SERVICES_NETWORK_RESOURCE_SCHEDULER_PARAMS_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "net/nqe/effective_connection_type.h"

namespace network {

// Parses and stores the resource scheduler parameters based on the set of
// enabled experiments and/or estimated network quality.
class COMPONENT_EXPORT(NETWORK_SERVICE) ResourceSchedulerParamsManager {
 public:
  // A struct that stores the resource scheduler parameters that vary with
  // network quality.
  struct COMPONENT_EXPORT(NETWORK_SERVICE) ParamsForNetworkQuality {
    ParamsForNetworkQuality();

    ParamsForNetworkQuality(size_t max_delayable_requests,
                            double non_delayable_weight,
                            bool delay_requests_on_multiplexed_connections,
                            base::Optional<base::TimeDelta> max_queuing_time);

    ParamsForNetworkQuality(const ParamsForNetworkQuality& other);

    // The maximum number of delayable requests allowed.
    size_t max_delayable_requests;

    // The weight of a non-delayable request when counting the effective number
    // of non-delayable requests in-flight.
    double non_delayable_weight;

    // True if requests to servers that support prioritization (e.g.,
    // H2/SPDY/QUIC) should be delayed similar to other HTTP 1.1 requests.
    bool delay_requests_on_multiplexed_connections;

    // The maximum duration for which a request is queued after after which the
    // request is dispatched to the network.
    base::Optional<base::TimeDelta> max_queuing_time;
  };

  ResourceSchedulerParamsManager();
  ResourceSchedulerParamsManager(const ResourceSchedulerParamsManager& other);

  // Mapping from the observed Effective Connection Type (ECT) to
  // ParamsForNetworkQuality.
  typedef std::map<net::EffectiveConnectionType, ParamsForNetworkQuality>
      ParamsForNetworkQualityContainer;

  // Constructor to be used when ParamsForNetworkQualityContainer need to be
  // overwritten.
  explicit ResourceSchedulerParamsManager(
      const ParamsForNetworkQualityContainer&
          params_for_network_quality_container);

  ~ResourceSchedulerParamsManager();

  // Returns the parameters for resource loading based on
  // |effective_connection_type|. Virtual for testing.
  ParamsForNetworkQuality GetParamsForEffectiveConnectionType(
      net::EffectiveConnectionType effective_connection_type) const;

  // Resets the internal container with the given one.
  void Reset(const ParamsForNetworkQualityContainer&
                 params_for_network_quality_container) {
    params_for_network_quality_container_ =
        params_for_network_quality_container;
  }
  void Reset(const ResourceSchedulerParamsManager& other) {
    Reset(other.params_for_network_quality_container_);
  }

 private:
  // The number of delayable requests in-flight for different ranges of the
  // network quality.
  ParamsForNetworkQualityContainer params_for_network_quality_container_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_RESOURCE_SCHEDULER_PARAMS_MANAGER_H_
