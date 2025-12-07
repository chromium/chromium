// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_pool_additional_capacity.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "net/base/features.h"

namespace net {

static constexpr char kErrorHistogramName[] =
    "Net.TcpSocketPoolLimitRandomization.Error";

// These values are persisted to logs, entries should not be renumbered.
// LINT.IfChange(SocketPoolAdditionalCapacityError)
enum class SocketPoolAdditionalCapacityError {
  kInvalidBase = 0,
  kInvalidCapacity = 1,
  kInvalidMinimum = 2,
  kInvalidNoise = 3,
  kInvalidSocketSoftCap = 4,
  kInvalidSocketInUse = 5,
  kInvalidSocketAllocation = 6,
  kMaxValue = kInvalidSocketAllocation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:SocketPoolAdditionalCapacityError)

// static
SocketPoolAdditionalCapacity SocketPoolAdditionalCapacity::Create() {
  if (base::FeatureList::IsEnabled(
          features::kTcpSocketPoolLimitRandomization)) {
    return SocketPoolAdditionalCapacity(
        features::kTcpSocketPoolLimitRandomizationBase.Get(),
        features::kTcpSocketPoolLimitRandomizationCapacity.Get(),
        features::kTcpSocketPoolLimitRandomizationMinimum.Get(),
        features::kTcpSocketPoolLimitRandomizationNoise.Get());
  }
  return SocketPoolAdditionalCapacity();
}

SocketPoolAdditionalCapacity SocketPoolAdditionalCapacity::CreateEmpty() {
  return SocketPoolAdditionalCapacity();
}

// static
SocketPoolAdditionalCapacity SocketPoolAdditionalCapacity::CreateForTest(
    double base,
    size_t capacity,
    double minimum,
    double noise) {
  return SocketPoolAdditionalCapacity(base, capacity, minimum, noise);
}

SocketPoolState SocketPoolAdditionalCapacity::NextStateBeforeAllocation(
    SocketPoolState current_state,
    size_t sockets_in_use,
    size_t socket_soft_cap) const {
  SocketPoolState next_state = NextStateBeforeAllocationImpl(
      current_state, sockets_in_use, socket_soft_cap);
  LogStateTransition(SocketPoolAction::kAllocation, current_state, next_state,
                     sockets_in_use);
  return next_state;
}

SocketPoolState SocketPoolAdditionalCapacity::NextStateAfterRelease(
    SocketPoolState current_state,
    size_t sockets_in_use,
    size_t socket_soft_cap) const {
  SocketPoolState next_state =
      NextStateAfterReleaseImpl(current_state, sockets_in_use, socket_soft_cap);
  LogStateTransition(SocketPoolAction::kRelease, current_state, next_state,
                     sockets_in_use);
  return next_state;
}

// static
void SocketPoolAdditionalCapacity::LogStateTransition(
    SocketPoolAction action,
    SocketPoolState current_state,
    SocketPoolState next_state,
    size_t sockets_in_use) {
  base::UmaHistogramCounts1000(
      base::StringPrintf(
          "Net.TcpSocketPoolLimitRandomization.Transition.%s.%sTo%s",
          action == SocketPoolAction::kAllocation ? "Allocation" : "Release",
          current_state == SocketPoolState::kUncapped ? "Uncapped" : "Capped",
          next_state == SocketPoolState::kUncapped ? "Uncapped" : "Capped"),
      sockets_in_use);
}

SocketPoolAdditionalCapacity::SocketPoolAdditionalCapacity(double base,
                                                           size_t capacity,
                                                           double minimum,
                                                           double noise)
    : base_(base), capacity_(capacity), minimum_(minimum), noise_(noise) {
  bool is_invalid = false;
  if (base_ < 0.0 || base_ > 1.0 || std::isnan(base_)) {
    base::UmaHistogramEnumeration(
        kErrorHistogramName, SocketPoolAdditionalCapacityError::kInvalidBase);
    is_invalid = true;
  }
  if (capacity_ > 256) {
    base::UmaHistogramEnumeration(
        kErrorHistogramName,
        SocketPoolAdditionalCapacityError::kInvalidCapacity);
    is_invalid = true;
  }
  if (minimum_ < 0.0 || minimum_ > 1.0 || std::isnan(minimum_)) {
    base::UmaHistogramEnumeration(
        kErrorHistogramName,
        SocketPoolAdditionalCapacityError::kInvalidMinimum);
    is_invalid = true;
  }
  if (noise_ < 0.0 || noise_ > 1.0 || std::isnan(noise_)) {
    base::UmaHistogramEnumeration(
        kErrorHistogramName, SocketPoolAdditionalCapacityError::kInvalidNoise);
    is_invalid = true;
  }
  // If any part of the config is invalid we should prevent use of additional
  // socket space to ensure no impact to browser functionality.
  if (is_invalid) {
    base_ = 0.0;
    capacity_ = 0;
    minimum_ = 0.0;
    noise_ = 0.0;
  }
}

SocketPoolState SocketPoolAdditionalCapacity::NextStateBeforeAllocationImpl(
    SocketPoolState current_state,
    size_t sockets_in_use,
    size_t socket_soft_cap) const {
  std::optional<SocketPoolState> common_state =
      NextStateCommonImpl(sockets_in_use, socket_soft_cap);
  if (common_state) {
    return *common_state;
  }

  // As we are using additional capacity, a socket allocation cannot transition
  // the pool to be uncapped.
  if (current_state == SocketPoolState::kCapped) {
    return SocketPoolState::kCapped;
  }

  // At this point we know we are uncapped and are using more sockets than the
  // soft cap, so we calculate the probability using the amount of free capacity
  // so the probability exponentially converges to 1 as capacity goes to 0.
  return ShouldTransitionState(SocketPoolAction::kAllocation,
                               socket_soft_cap + capacity_ - sockets_in_use)
             ? SocketPoolState::kCapped
             : SocketPoolState::kUncapped;
}

SocketPoolState SocketPoolAdditionalCapacity::NextStateAfterReleaseImpl(
    SocketPoolState current_state,
    size_t sockets_in_use,
    size_t socket_soft_cap) const {
  std::optional<SocketPoolState> common_state =
      NextStateCommonImpl(sockets_in_use, socket_soft_cap);
  if (common_state) {
    return *common_state;
  }

  // As we are reclaiming capacity, a socket release cannot transition the pool
  // to be capped.
  if (current_state == SocketPoolState::kUncapped) {
    return SocketPoolState::kUncapped;
  }

  // At this point we know we are capped and are using more sockets than the
  // soft cap, so we calculate the probability using the amount of used capacity
  // so the probability exponentially converges to 1 as usage goes to 0.
  return ShouldTransitionState(SocketPoolAction::kRelease,
                               sockets_in_use - socket_soft_cap)
             ? SocketPoolState::kUncapped
             : SocketPoolState::kCapped;
}

std::optional<SocketPoolState>
SocketPoolAdditionalCapacity::NextStateCommonImpl(
    size_t sockets_in_use,
    size_t socket_soft_cap) const {
  // We don't want to throw in this code, so for range errors we simply log and
  // cap the pool to prevent overallocation of sockets.
  if (!base::IsValueInRangeForNumericType<uint16_t>(socket_soft_cap)) {
    base::UmaHistogramEnumeration(
        kErrorHistogramName,
        SocketPoolAdditionalCapacityError::kInvalidSocketSoftCap);
    return SocketPoolState::kCapped;
  }
  if (!base::IsValueInRangeForNumericType<uint16_t>(sockets_in_use)) {
    base::UmaHistogramEnumeration(
        kErrorHistogramName,
        SocketPoolAdditionalCapacityError::kInvalidSocketInUse);
    return SocketPoolState::kCapped;
  }

  // At this point we know all three numbers are below an uint16_t, so there's
  // no risk doing math with them.

  // We cannot allow more sockets than the maximum allowed to be in use.
  if (sockets_in_use > (socket_soft_cap + capacity_)) {
    base::UmaHistogramEnumeration(
        kErrorHistogramName,
        SocketPoolAdditionalCapacityError::kInvalidSocketAllocation);
    return SocketPoolState::kCapped;
  }

  // If we are using fewer sockets than the soft cap we are always uncapped.
  if (sockets_in_use < socket_soft_cap) {
    return SocketPoolState::kUncapped;
  }

  // If we are using all possible sockets we are always capped.
  if (sockets_in_use == (socket_soft_cap + capacity_)) {
    return SocketPoolState::kCapped;
  }

  // Otherwise, the logic is not shared.
  return std::nullopt;
}

bool SocketPoolAdditionalCapacity::ShouldTransitionState(
    SocketPoolAction action,
    size_t actions_taken) const {
  // We need to enforce bounds before any math is done here.
  CHECK_GE(base_, 0.0);
  CHECK_LE(base_, 1.0);
  CHECK_LE(capacity_, 256u);
  CHECK_GE(minimum_, 0.0);
  CHECK_LE(minimum_, 1.0);
  CHECK_GE(noise_, 0.0);
  CHECK_LE(noise_, 1.0);
  CHECK_LE(actions_taken, capacity_);

  // First, we determine the percentage of the actions remaining.
  double percentage_used = actions_taken / static_cast<double>(capacity_);

  // Second, we calculate the random noise we want to apply.
  double noise_multiple =
      1.0 + noise_ * base::RandDouble() * (base::RandBool() ? 1 : -1);

  // Third, we calculate the unbound probability.
  double unbound_probability =
      noise_multiple * std::pow(base_, percentage_used);

  // Fourth, we calculate and log the bound probability.
  double bound_probability =
      std::max(minimum_, std::min(1.0, unbound_probability));
  base::UmaHistogramPercentage(
      base::StringPrintf(
          "Net.TcpSocketPoolLimitRandomization.Probability.%s",
          action == SocketPoolAction::kAllocation ? "Allocation" : "Release"),
      static_cast<int>(bound_probability * 100));

  // Finally, we return the result.
  return base::RandDouble() < bound_probability;
}

}  // namespace net
