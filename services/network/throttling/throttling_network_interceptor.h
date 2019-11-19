// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_INTERCEPTOR_H_
#define SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_INTERCEPTOR_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/timer/timer.h"

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace network {
class NetworkConditions;

// ThrottlingNetworkInterceptor emulates network conditions for transactions
// with specific client id.
class COMPONENT_EXPORT(NETWORK_SERVICE) ThrottlingNetworkInterceptor {
 public:
  using ThrottleCallback = base::Callback<void(int, int64_t)>;

  ThrottlingNetworkInterceptor();
  virtual ~ThrottlingNetworkInterceptor();

  base::WeakPtr<ThrottlingNetworkInterceptor> GetWeakPtr();

  // Applies network emulation configuration.
  void UpdateConditions(std::unique_ptr<NetworkConditions> conditions);

  // Throttles with |is_upload == true| always succeed, even in offline mode.
  int StartThrottle(int result,
                    int64_t bytes,
                    base::TimeTicks send_end,
                    bool start,
                    bool is_upload,
                    const ThrottleCallback& callback);
  void StopThrottle(const ThrottleCallback& callback);

  bool IsOffline();

 private:
  struct ThrottleRecord {
   public:
    ThrottleRecord();
    ThrottleRecord(const ThrottleRecord& other);
    ~ThrottleRecord();
    int result;
    int64_t bytes;
    int64_t send_end;
    bool is_upload;
    ThrottleCallback callback;
  };
  using ThrottleRecords = std::vector<ThrottleRecord>;

  void FinishRecords(ThrottleRecords* records, bool offline);

  uint64_t UpdateThrottledRecords(base::TimeTicks now,
                                  ThrottleRecords* records,
                                  uint64_t last_tick,
                                  base::TimeDelta tick_length);
  void UpdateThrottled(base::TimeTicks now);
  void UpdateSuspended(base::TimeTicks now);

  void CollectFinished(ThrottleRecords* records, ThrottleRecords* finished);
  void OnTimer();

  base::TimeTicks CalculateDesiredTime(const ThrottleRecords& records,
                                       uint64_t last_tick,
                                       base::TimeDelta tick_length);
  void ArmTimer(base::TimeTicks now);

  void RemoveRecord(ThrottleRecords* records, const ThrottleCallback& callback);

  std::unique_ptr<NetworkConditions> conditions_;

  // Throttables suspended for a "latency" period.
  ThrottleRecords suspended_;

  // Throttables waiting certain amount of transfer to be "accounted".
  ThrottleRecords download_;
  ThrottleRecords upload_;

  base::OneShotTimer timer_;
  base::TimeTicks offset_;
  base::TimeDelta download_tick_length_;
  base::TimeDelta upload_tick_length_;
  base::TimeDelta latency_length_;
  uint64_t download_last_tick_;
  uint64_t upload_last_tick_;

  base::WeakPtrFactory<ThrottlingNetworkInterceptor> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ThrottlingNetworkInterceptor);
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_INTERCEPTOR_H_
