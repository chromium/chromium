// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_INTERCEPTOR_H_
#define SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_INTERCEPTOR_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "services/network/throttling/network_conditions.h"

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
  using ThrottleCallback = base::RepeatingCallback<void(int, int64_t)>;

  ThrottlingNetworkInterceptor();

  ThrottlingNetworkInterceptor(const ThrottlingNetworkInterceptor&) = delete;
  ThrottlingNetworkInterceptor& operator=(const ThrottlingNetworkInterceptor&) =
      delete;

  virtual ~ThrottlingNetworkInterceptor();

  base::WeakPtr<ThrottlingNetworkInterceptor> GetWeakPtr();

  // Applies network emulation configuration.
  void UpdateConditions(const NetworkConditions& conditions);

  // This function implements throttling logic. It is meant to be called after
  // the interaction with a real network to delay invocation of a client
  // callback.
  // * 'result' holds the result of a real network operation from
  //    net/base/net_error_list.h.
  // * 'bytes' is the amount of data transferred over the network. It is used
  //    to calculate the delay.
  // * 'send_end' is the time when the the real network operation was finished.
  // * 'start' is true if this is invoked for starting HTTP transaction (as
  //    opposed to other operations such as reading ressponse or uploading
  //    data).
  // * 'is_upload' is true if this is invoked for sending data over the network
  //    (as opposed to reading data from the network). NetworkConditions could
  //    specify different speed for upload and download and this parameter is
  //    used to pick the right speed.
  // * 'callback' is what needs to be invoked later, if extra delay is
  //    introduced to emulate a slow network.
  //
  // This function returns net::ERR_IO_PENDING if the delay is introduced and
  // 'callack' will be invoked after this delay.
  // When no throttling is needed, this function simply returns 'result' and
  // ignores the rest of the arguments including 'callback'.
  // The same happens if 'result' corresponds to an error (i.e. is negative
  // meaning that the real network operation has failed).
  // When emulating offline network, the function returns
  // net::ERR_INTERNET_DISCONNECTED and also ignores the arguments including
  // 'callback'.
  // Note, however, that if 'is_upload' is true the function will
  // return 'result' insead of net::ERR_INTERNET_DISCONNECTED. (But why?)
  int StartThrottle(int result,
                    int64_t bytes,
                    base::TimeTicks send_end,
                    bool start,
                    bool is_upload,
                    const ThrottleCallback& callback);

  // Cancels throttling, previously started with the given 'callback'.
  // This is useful to call if the client is being destructed or otherwise is
  // no longer interested in throttling.
  void StopThrottle(const ThrottleCallback& callback);

  // Whether offline network is emulated.
  bool IsOffline();

  void SetSuspendWhenOffline(bool suspend);

  // Calculates buffer len to pass to network transaction Read call.
  int GetReadBufLen(int buf_len) const;

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

  NetworkConditions conditions_;

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
  bool suspend_when_offline_ = false;

  base::WeakPtrFactory<ThrottlingNetworkInterceptor> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_THROTTLING_NETWORK_INTERCEPTOR_H_
