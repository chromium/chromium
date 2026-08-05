// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_CONNECT_PREDICTOR_H_
#define NET_DNS_CONNECT_PREDICTOR_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "net/base/ip_address.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"

namespace net {

// A predictor for the error or source address that will result from a call to
// connect() on a UDP socket to a given destination. This can be used instead of
// actually connecting.
class NET_EXPORT_PRIVATE ConnectPredictor {
 public:
  // The result of a UDP connect() attempt to a specific destination IP.
  struct ConnectResult {
    // Errors here only reflect the state of the routing table, not the state of
    // the remote host, so it is safe to reuse this cached value as long as the
    // routing table has not changed.
    int rv;
    IPAddress source_address;
  };

  // A single partition of the cache.
  class Partition {
   public:
    Partition() = default;

    Partition(const Partition&) = delete;
    Partition& operator=(const Partition&) = delete;

    // Returns the predicted result of  a connect() attempt to the given
    // destination.
    virtual std::optional<ConnectResult> Predict(
        const IPAddress& destination) = 0;

    // Records the result of an actual connect() attempt to the given
    // destination so that it can be used for future predictions.
    virtual void RecordResult(const IPAddress& destination,
                              const ConnectResult& result) = 0;

   protected:
    // A Partition is never deleted via its base pointer.
    ~Partition() = default;
  };

  // Returns a concrete instance of ConnectPredictor that is enabled and will
  // work on the current platform, or nullptr if there is none.
  static std::unique_ptr<ConnectPredictor> Create();

  ConnectPredictor() = default;
  ConnectPredictor(const ConnectPredictor&) = delete;
  ConnectPredictor& operator=(const ConnectPredictor&) = delete;

  virtual ~ConnectPredictor() = default;

  // Returns the cache partition for a given `handle` and `nak`. Creates the
  // partition if it doesn't already exist.
  virtual base::WeakPtr<Partition> GetPartition(
      handles::NetworkHandle handle,
      NetworkAnonymizationKey nak) = 0;
};

}  // namespace net

#endif  // NET_DNS_CONNECT_PREDICTOR_H_
