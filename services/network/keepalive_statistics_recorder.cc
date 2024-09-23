// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/keepalive_statistics_recorder.h"

#include <algorithm>

#include "base/not_fatal_until.h"

namespace network {

KeepaliveStatisticsRecorder::KeepaliveStatisticsRecorder() = default;
KeepaliveStatisticsRecorder::~KeepaliveStatisticsRecorder() = default;

void KeepaliveStatisticsRecorder::Register(
    const base::UnguessableToken& top_level_frame_id) {
  auto it = per_top_level_frame_records_.find(top_level_frame_id);
  if (it == per_top_level_frame_records_.end()) {
    per_top_level_frame_records_.insert(
        std::make_pair(top_level_frame_id, PerTopLevelFrameStats()));
    return;
  }

  ++it->second.num_registrations;
}

void KeepaliveStatisticsRecorder::Unregister(
    const base::UnguessableToken& top_level_frame_id) {
  auto it = per_top_level_frame_records_.find(top_level_frame_id);
  CHECK(it != per_top_level_frame_records_.end(), base::NotFatalUntil::M130);

  if (it->second.num_registrations == 1) {
    per_top_level_frame_records_.erase(it);
    return;
  }
  --it->second.num_registrations;
}

void KeepaliveStatisticsRecorder::OnLoadStarted(
    const base::UnguessableToken& top_level_frame_id,
    int request_size) {
  auto it = per_top_level_frame_records_.find(top_level_frame_id);
  if (it != per_top_level_frame_records_.end()) {
    ++it->second.num_inflight_requests;
    it->second.total_request_size += request_size;
    if (it->second.peak_inflight_requests < it->second.num_inflight_requests) {
      it->second.peak_inflight_requests = it->second.num_inflight_requests;
    }
  }
  ++num_inflight_requests_;
  if (peak_inflight_requests_ < num_inflight_requests_) {
    peak_inflight_requests_ = num_inflight_requests_;
  }
}

void KeepaliveStatisticsRecorder::OnLoadFinished(
    const base::UnguessableToken& top_level_frame_id,
    int request_size) {
  auto it = per_top_level_frame_records_.find(top_level_frame_id);
  if (it != per_top_level_frame_records_.end()) {
    --it->second.num_inflight_requests;
    DCHECK_GE(it->second.total_request_size, request_size);
    it->second.total_request_size -= request_size;
  }
  --num_inflight_requests_;
}

int KeepaliveStatisticsRecorder::NumInflightRequestsPerTopLevelFrame(
    const base::UnguessableToken& top_level_frame_id) const {
  auto it = per_top_level_frame_records_.find(top_level_frame_id);
  if (it == per_top_level_frame_records_.end())
    return 0;
  return it->second.num_inflight_requests;
}

int KeepaliveStatisticsRecorder::GetTotalRequestSizePerTopLevelFrame(
    const base::UnguessableToken& top_level_frame_id) const {
  auto it = per_top_level_frame_records_.find(top_level_frame_id);
  if (it == per_top_level_frame_records_.end())
    return 0;
  return it->second.total_request_size;
}

}  // namespace network
