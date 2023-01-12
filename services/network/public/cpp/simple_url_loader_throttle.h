// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_THROTTLE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_THROTTLE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace network {

constexpr char kBatchSimpleURLLoaderEnabledTrafficAnnotationHashesParam[] =
    "batching_enabled_traffic_annotation_hashes";

// Throttles SimpleURLLoader creations based on the underlying network
// connection status. When a SimpleURLLoader is allowed to be batched, this
// class hooks the the SimpleURLLoader creation and delays the SimpleURLLoader
// until the underlying network connection becomes active, or a certain amount
// of time has elapsed.
class COMPONENT_EXPORT(NETWORK_CPP) SimpleURLLoaderThrottle {
 public:
  // Returns true when batching a SimpleURLLoader associated with
  // `traffic_annotation` is enabled via experiment configurations.
  static bool IsBatchingEnabled(
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Resets experiment configurations for testing.
  static void ResetConfigForTesting();

  // Handles platform specific logic to determine whether a request should be
  // batched or not. Also used for testing.
  class COMPONENT_EXPORT(NETWORK_CPP) Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Returns whether a request should be throttled.
    virtual bool ShouldThrottle() = 0;
  };

  SimpleURLLoaderThrottle();
  ~SimpleURLLoaderThrottle();
  SimpleURLLoaderThrottle(const SimpleURLLoaderThrottle&) = delete;
  SimpleURLLoaderThrottle& operator=(const SimpleURLLoaderThrottle&) = delete;
  SimpleURLLoaderThrottle(SimpleURLLoaderThrottle&&) = delete;
  SimpleURLLoaderThrottle& operator=(SimpleURLLoaderThrottle&&) = delete;

  // Called before starting a SimpleURLLoader which can be batched. The loader
  // will be suspended if Delegate::ShouldThrottle() returns true.
  void NotifyWhenReady(base::OnceClosure callback);

  // Resumes the throttled callback if needed.
  void OnReadyToStart();

  void SetDelegateForTesting(std::unique_ptr<Delegate> delegate);
  void SetTimeoutForTesting(base::TimeDelta timeout);
  Delegate& GetDelegateForTesting() { return *delegate_; }

 private:
  void OnTimeout();

  std::unique_ptr<Delegate> delegate_;
  base::OnceClosure callback_;

  static constexpr base::TimeDelta kDefaultTimeout = base::Minutes(15);
  base::TimeDelta timeout_ = kDefaultTimeout;
  base::OneShotTimer timeout_timer_;

  base::TimeTicks throttling_start_time_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_THROTTLE_H_
