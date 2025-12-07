// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_POOL_ADDITIONAL_CAPACITY_H_
#define NET_SOCKET_SOCKET_POOL_ADDITIONAL_CAPACITY_H_

#include <string>

#include "base/strings/stringprintf.h"
#include "net/base/net_export.h"

namespace net {

// Socket pools update their state before every socket allocation and release.
enum class SocketPoolState {

  // Uncapped pools can allocate or release sockets.
  kUncapped,

  // Capped pools cannot allocate sockets, but can release them.
  kCapped
};

// This class encapsulates the logic for the additional TCP Socket Pool capacity
// allocated (and randomized) to prevent cross-site state tracking.
// See crbug.com/415691664 for more details.
class NET_EXPORT_PRIVATE SocketPoolAdditionalCapacity {
 public:
  // This initializes using values from kTcpSocketPoolLimitRandomization.
  static SocketPoolAdditionalCapacity Create();

  // This initializes an empty pool that contains no capacity.
  static SocketPoolAdditionalCapacity CreateEmpty();

  static SocketPoolAdditionalCapacity CreateForTest(double base,
                                                    size_t capacity,
                                                    double minimum,
                                                    double noise);

  // Calculates the next `SocketPoolState` before the allocation of a socket.
  // `sockets_in_use` should be counted pre-allocation and `socket_soft_cap`
  // is likely being passed down from `g_max_sockets_per_pool`.
  SocketPoolState NextStateBeforeAllocation(SocketPoolState current_state,
                                            size_t sockets_in_use,
                                            size_t socket_soft_cap) const;

  // Calculates the next `SocketPoolState` after the release of a socket.
  // `sockets_in_use` should be counted post-release and `socket_soft_cap`
  // is likely being passed down from `g_max_sockets_per_pool`.
  SocketPoolState NextStateAfterRelease(SocketPoolState current_state,
                                        size_t sockets_in_use,
                                        size_t socket_soft_cap) const;

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
  enum class SocketPoolAction { kAllocation, kRelease };

  static void LogStateTransition(SocketPoolAction action,
                                 SocketPoolState current_state,
                                 SocketPoolState next_state,
                                 size_t sockets_in_use);

  SocketPoolAdditionalCapacity() = default;
  SocketPoolAdditionalCapacity(double base,
                               size_t capacity,
                               double minimum,
                               double noise);

  // Helper for NextStateBeforeAllocation to avoid duplicate logging code.
  SocketPoolState NextStateBeforeAllocationImpl(SocketPoolState current_state,
                                                size_t sockets_in_use,
                                                size_t socket_soft_cap) const;

  // Helper for NextStateAfterRelease to avoid duplicate logging code.
  SocketPoolState NextStateAfterReleaseImpl(SocketPoolState current_state,
                                            size_t sockets_in_use,
                                            size_t socket_soft_cap) const;

  // This helper function for `NextStateBefore(Allocation|Release)` handles
  // common logic. Returns a SocketPoolState if the common logic is controlling,
  // and std::nullopt otherwise.
  std::optional<SocketPoolState> NextStateCommonImpl(
      size_t sockets_in_use,
      size_t socket_soft_cap) const;

  // Unlike other functions in this class, this one will CHECK on invalid
  // constants and inputs. As such, all validation must be performed before we
  // get to this stage. The actual way this function rolls dice are quite
  // complex, please see the implementation for details.
  // `actions_taken` must be between 0 and `capacity_`, and is the
  // amount of `capacity_` already allocated for `NextStateBeforeAllocationImpl`
  // and the amount of `capacity_` free for `NextStateAfterReleaseImpl`. This is
  // done to ensure the probability converges toward 1 correctly for each.
  bool ShouldTransitionState(SocketPoolAction action,
                             size_t actions_taken) const;

  // See the implementation of `ShouldTransitionState` for how these constants
  // are used and bound in calculating the probability of a state transition.
  double base_ = 0.0;
  size_t capacity_ = 0;
  double minimum_ = 0.0;
  double noise_ = 0.0;
};

}  // namespace net

#endif  // NET_SOCKET_SOCKET_POOL_ADDITIONAL_CAPACITY_H_
