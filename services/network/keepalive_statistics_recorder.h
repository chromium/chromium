// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_KEEPALIVE_STATISTICS_RECORDER_H_
#define SERVICES_NETWORK_KEEPALIVE_STATISTICS_RECORDER_H_

#include <unordered_map>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace network {

// KeepaliveStatisticsRecorder keeps tracks of the number of inflight requests
// with "keepalive" set and records UMA histograms.
class COMPONENT_EXPORT(NETWORK_SERVICE) KeepaliveStatisticsRecorder
    : public base::SupportsWeakPtr<KeepaliveStatisticsRecorder> {
 public:
  struct PerProcessStats {
    int num_registrations = 1;
    int num_inflight_requests = 0;
    int peak_inflight_requests = 0;
    int total_request_size = 0;
  };

  KeepaliveStatisticsRecorder();
  ~KeepaliveStatisticsRecorder();

  // Registers / Unregisters |process_id| to this object. There can be multiple
  // Register / Unregister calls with the same |process_id|, and this object
  // thinks a process |p| is gone when the number of Register calls with |p|
  // equals to the number of Unregister calls with |p|.
  void Register(int process_id);
  void Unregister(int process_id);

  // Called when a request with keepalive set starts.
  void OnLoadStarted(int process_id, int request_size);
  // Called when a request with keepalive set finishes.
  void OnLoadFinished(int process_id, int request_size);

  const std::unordered_map<int, PerProcessStats>& per_process_records() const {
    return per_process_records_;
  }
  int NumInflightRequestsPerProcess(int process_id) const;
  int GetTotalRequestSizePerProcess(int process_id) const;
  int num_inflight_requests() const { return num_inflight_requests_; }
  int peak_inflight_requests() const { return peak_inflight_requests_; }

 private:
  std::unordered_map<int, PerProcessStats> per_process_records_;
  int num_inflight_requests_ = 0;
  int peak_inflight_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(KeepaliveStatisticsRecorder);
};

}  // namespace network

#endif  // SERVICES_NETWORK_KEEPALIVE_STATISTICS_RECORDER_H_
