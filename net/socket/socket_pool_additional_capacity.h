// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_POOL_ADDITIONAL_CAPACITY_H_
#define NET_SOCKET_SOCKET_POOL_ADDITIONAL_CAPACITY_H_

#include <string>

#include "base/strings/stringprintf.h"
#include "net/base/net_export.h"

namespace net {

// This class encapsulates the logic for the additional TCP Socket Pool capacity
// allocated (and randomized) to prevent cross-site state tracking.
// See crbug.com/415691664 for more details.
class NET_EXPORT_PRIVATE SocketPoolAdditionalCapacity {
 public:
  // This initializes using values from kTcpSocketPoolLimitRandomization.
  static SocketPoolAdditionalCapacity Create();

  static SocketPoolAdditionalCapacity CreateForTest(double base,
                                                    int capacity,
                                                    double minimum,
                                                    double noise);

  explicit operator std::string() const {
    return base::StringPrintf(
        "SocketPoolAdditionalCapacity(base:%e,capacity:%i,minimum:%e,noise:%e)",
        base_, capacity_, minimum_, noise_);
  }

  friend bool operator==(const SocketPoolAdditionalCapacity& lhs,
                         const SocketPoolAdditionalCapacity& rhs) {
    return lhs.base_ == rhs.base_ && lhs.capacity_ == rhs.capacity_ &&
           lhs.minimum_ == rhs.minimum_ && lhs.noise_ == rhs.noise_;
  }

 private:
  SocketPoolAdditionalCapacity() = default;
  SocketPoolAdditionalCapacity(double base,
                               int capacity,
                               double minimum,
                               double noise)
      : base_(base), capacity_(capacity), minimum_(minimum), noise_(noise) {}

  double base_ = 0.0;
  int capacity_ = 0;
  double minimum_ = 0.0;
  double noise_ = 0.0;
};

}  // namespace net

#endif  // NET_SOCKET_SOCKET_POOL_ADDITIONAL_CAPACITY_H_
