// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_CLUSTER_BUILDER_H_
#define MEDIA_FORMATS_WEBM_CLUSTER_BUILDER_H_

#include <stdint.h>

#include <memory>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/numerics/safe_conversions.h"

namespace media {

class Cluster {
 public:
  Cluster() = delete;

  // The size of the `bytes_used` might be less size of `data`.
  Cluster(base::HeapArray<uint8_t> data, int bytes_used);

  Cluster(const Cluster&) = delete;
  Cluster& operator=(const Cluster&) = delete;

  ~Cluster();

  int bytes_used() const { return bytes_used_; }

  // Returns a span over the `bytes_used()` valid bytes of the cluster.
  base::span<const uint8_t> AsSpan() const {
    return data_.first(base::checked_cast<size_t>(bytes_used_));
  }

 private:
  base::HeapArray<uint8_t> data_;
  const int bytes_used_;
};

class ClusterBuilder {
 public:
  ClusterBuilder();

  ClusterBuilder(const ClusterBuilder&) = delete;
  ClusterBuilder& operator=(const ClusterBuilder&) = delete;

  ~ClusterBuilder();

  void SetClusterTimecode(int64_t cluster_timecode);
  void AddSimpleBlock(int track_num,
                      int64_t timecode,
                      int flags,
                      base::span<const uint8_t> data);
  void AddBlockGroup(int track_num,
                     int64_t timecode,
                     int duration,
                     int flags,
                     bool is_key_frame,
                     base::span<const uint8_t> data);
  void AddBlockGroupWithoutBlockDuration(int track_num,
                                         int64_t timecode,
                                         int flags,
                                         bool is_key_frame,
                                         base::span<const uint8_t> data);

  std::unique_ptr<Cluster> Finish();
  std::unique_ptr<Cluster> FinishWithUnknownSize();

 private:
  void AddBlockGroupInternal(int track_num,
                             int64_t timecode,
                             bool include_block_duration,
                             int duration,
                             int flags,
                             bool is_key_frame,
                             base::span<const uint8_t> data);
  void Reset();
  void ExtendBuffer(size_t bytes_needed);
  void UpdateUInt64(size_t offset, int64_t value);
  void WriteBlock(base::SpanWriter<uint8_t>& writer,
                  int track_num,
                  int64_t timecode,
                  int flags,
                  base::span<const uint8_t> data);

  base::HeapArray<uint8_t> buffer_;
  size_t bytes_used_;
  int64_t cluster_timecode_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_CLUSTER_BUILDER_H_
