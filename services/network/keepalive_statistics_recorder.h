// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_KEEPALIVE_STATISTICS_RECORDER_H_
#define SERVICES_NETWORK_KEEPALIVE_STATISTICS_RECORDER_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"

namespace network {

// KeepaliveStatisticsRecorder keeps tracks of the number of inflight requests
// with "keepalive" set.
class COMPONENT_EXPORT(NETWORK_SERVICE) KeepaliveStatisticsRecorder final {
 public:
  struct PerTopLevelFrameStats {
    int num_registrations = 1;
    int num_inflight_requests = 0;
    int peak_inflight_requests = 0;
    int total_request_size = 0;
  };

  KeepaliveStatisticsRecorder();

  KeepaliveStatisticsRecorder(const KeepaliveStatisticsRecorder&) = delete;
  KeepaliveStatisticsRecorder& operator=(const KeepaliveStatisticsRecorder&) =
      delete;

  ~KeepaliveStatisticsRecorder();

  // Registers / Unregisters |top_level_frame| to this object.
  // There can be multiple Register / Unregister calls with the same
  // |top_level_frame|, and this object thinks a an entry for |top_level_frame|
  // is gone when the number of Register calls with |top_level_frame| equals to
  // the number of Unregister calls with |token|.
  void Register(const base::UnguessableToken& top_level_frame_id);
  void Unregister(const base::UnguessableToken& top_level_frame_id);

  // Called when a request with keepalive set starts.
  void OnLoadStarted(const base::UnguessableToken& top_level_frame_id,
                     int request_size);
  // Called when a request with keepalive set finishes.
  void OnLoadFinished(const base::UnguessableToken& top_level_frame_id,
                      int request_size);

  const std::map<base::UnguessableToken, PerTopLevelFrameStats>&
  per_top_level_frame_records() const {
    return per_top_level_frame_records_;
  }
  int NumInflightRequestsPerTopLevelFrame(
      const base::UnguessableToken& top_level_frame_id) const;
  int GetTotalRequestSizePerTopLevelFrame(
      const base::UnguessableToken& top_level_frame_id) const;
  int num_inflight_requests() const { return num_inflight_requests_; }
  int peak_inflight_requests() const { return peak_inflight_requests_; }

  base::WeakPtr<KeepaliveStatisticsRecorder> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::map<base::UnguessableToken, PerTopLevelFrameStats>
      per_top_level_frame_records_;
  int num_inflight_requests_ = 0;
  int peak_inflight_requests_ = 0;
  base::WeakPtrFactory<KeepaliveStatisticsRecorder> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_KEEPALIVE_STATISTICS_RECORDER_H_
